// context.cpp

#include "l3.h"  

#include "context.h" 
#include "tiles.h"

namespace L3 {

    void Context::add(std::unique_ptr<Instruction> instr) {
        instructions.push_back(std::move(instr));
    }

    void Context::add(std::vector<LivenessInfo>::iterator begin, 
                      std::vector<LivenessInfo>::iterator end) {
        liveAnalysisReport.assign(begin, end);
    }

    const std::vector<std::unique_ptr<Instruction>>& Context::get() const {
        return instructions;
    }

    bool Context::empty() const { return instructions.empty(); }
    
    size_t Context::size() const { return instructions.size(); }

    bool Context::is_terminated() const {
        if (instructions.empty()) return false;
        InstructionType t = instructions.back()->type;
        return t == InstructionType::Br
            || t == InstructionType::BrT
            || t == InstructionType::Return
            || t == InstructionType::ReturnT
            || t == InstructionType::AssignFromCall;
    }

    void Context::print_trees(bool debug) const {
        if (!debug) return;

        std::cerr << "=== Context Trees ===\n";
        std::cerr << "instructions: " << instructions.size() 
                << " trees: " << trees.size() 
                << " liveness: " << liveAnalysisReport.size() << "\n\n";

        for (int i = 0; i < (int)instructions.size(); i++) {
            std::cerr << "[" << i << "] " << instructions[i]->to_string();
            
            if (i < (int)trees.size()) {
                if (trees[i]) {
                    std::cerr << "     tree: " << tree_to_string(*trees[i]) << "\n";
                } else {
                    std::cerr << "     tree: nullptr\n";
                }
            } else {
                std::cerr << "     tree: (no tree entry)\n";
            }

            if (i < (int)liveAnalysisReport.size()) {
                std::cerr << "     in : { ";
                for (const auto& v : liveAnalysisReport[i].in)  std::cerr << v.to_string() << " ";
                std::cerr << "}\n";
                std::cerr << "     out: { ";
                for (const auto& v : liveAnalysisReport[i].out) std::cerr << v.to_string() << " ";
                std::cerr << "}\n";
            }
            std::cerr << "\n";
        }
    }

    void Context::build_tree() {
        for (auto& instr : instructions) {
            trees.push_back(instr->to_tree()); 
        }
    }

    void Context::merge_tree() {
        if (instructions.size() != trees.size()) {
            std::cerr << "ASSERT FAIL: instructions.size()=" << instructions.size()
                    << " trees.size()=" << trees.size() << "\n";
            for (int i = 0; i < (int)instructions.size(); i++) {
                std::cerr << "  instr[" << i << "] = " << instructions[i]->to_string();
            }
            assert(false);
        }

        if (instructions.size() != liveAnalysisReport.size()) {
            std::cerr << "ASSERT FAIL: instructions.size()=" << instructions.size()
                    << " liveAnalysisReport.size()=" << liveAnalysisReport.size() << "\n";
            assert(false);
        }

        bool changed = true;
        while (changed) {
            changed = false;
            
            // recompute liveness at start of every pass for each context. 
            liveAnalysisReport = compute_liveness(*this);

            for (int i = 0; i < (int)instructions.size(); i++) {
                print_trees(false);
                if (!trees[i]) continue;

                const auto& live = liveAnalysisReport[i];
                const auto  type = instructions[i]->type;

                // erase dead code: var is written but not in out set
                bool is_pure = (type == InstructionType::AssignFromS  ||
                                type == InstructionType::AssignFromOp ||
                                type == InstructionType::AssignFromCmp ||
                                type == InstructionType::AssignFromLoad);

                auto writes = instructions[i]->writes();
                bool result_unused = true;
                for (const auto& w : writes) {
                    if (live.out.count(w)) { result_unused = false; break; }
                }

                if (is_pure && result_unused) {
                    instructions.erase(instructions.begin() + i);
                    trees.erase(trees.begin() + i);
                    liveAnalysisReport.erase(liveAnalysisReport.begin() + i);
                    i--;
                    changed = true;
                    continue;
                }

                // find if there is anything to merge 
                std::set<Variable> diff;
                std::set_difference(liveAnalysisReport[i].out.begin(), liveAnalysisReport[i].out.end(),
                                    liveAnalysisReport[i].in.begin(),  liveAnalysisReport[i].in.end(),
                                    std::inserter(diff, diff.begin()));

                if (diff.empty()) continue;
                assert(diff.size() == 1);
                const Variable& defined = *diff.begin();

                auto find_death = [&](const Variable& var, int start) -> int {
                    for (int k = start; k < (int)liveAnalysisReport.size(); k++){
                        if (liveAnalysisReport[k].in.count(var) && !liveAnalysisReport[k].out.count(var)){
                            return k;
                        }
                    }
                    return (int)liveAnalysisReport.size();
                };

                int death = find_death(defined, i + 1);
                if (death >= (int)instructions.size()) continue;

                bool safe_to_erase = false;
                bool all_ok = true;

                for (int j = i + 1; j <= death; j++) {
                    bool is_redef = trees[j]
                        ? tree_writes(*trees[j]).count(defined)
                        : instructions[j]->writes().count(defined);

                    bool j_reads = trees[j]
                        ? tree_reads(*trees[j]).count(defined)
                        : instructions[j]->reads().count(defined);

                    if (j_reads) {
                        auto source_copy = clone_tree(*trees[i]);
                        auto merged = L3::merge_tree(std::move(source_copy), std::move(trees[j]));
                        if (merged) {
                            trees[j] = std::move(merged);
                            safe_to_erase = true;
                        } else {
                            all_ok = false;
                            break;
                        }
                    }

                    if (is_redef) break;
                }

                if (safe_to_erase && all_ok) {
                    instructions.erase(instructions.begin() + i);
                    trees.erase(trees.begin() + i);
                    liveAnalysisReport.erase(liveAnalysisReport.begin() + i);
                    i--;
                    changed = true;
                }
            }
        }

        // after the maximum merged tree possible, go shrink the trees 
        for (auto& t : trees) {
            if (t)
                t = std::move(shrink_tree(*t));
        }

        // combine or merge the last two if they are mergeable 
        print_trees(true);
    }

    void Context::aggregate_tree() {
        StoreTile storeTile;
        for (size_t i = 0; i < instructions.size(); i++) {
            if (!trees[i]) continue;
            if (instructions[i]->type == InstructionType::Store) {
                if (storeTile.match(*trees[i])) {
                    instructions[i] = storeTile.emit(*trees[i]);
                }
            }
        }
    }

} // namespace L3