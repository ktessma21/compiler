// context.cpp

#include "l3.h"  

#include "context.h" 
#include "tree.h"


namespace L3 {


    static std::string leaqMatch(const Variable& dest, const TreeNode& node) {
        // Expect node to be: Add(base, Mul(index, scale))  where scale ∈ {1,2,4,8}
        //   or any commutation of the operands.
        // Returns the L2 "w @ w w E" string on match, or "" on no match.

        auto* add = std::get_if<BinOpNode>(&node.data);
        if (!add || add->op != Op::Add) return "";

        // Try to match: base_side is a Variable, mul_side is Mul(var, const-scale).
        auto try_match = [](const TreeNode& base_side,
                            const TreeNode& mul_side,
                            std::string& base_out,
                            std::string& index_out,
                            long long& scale_out) -> bool {
            auto* base_var = std::get_if<Variable>(&base_side.data);
            if (!base_var) return false;

            auto* mul = std::get_if<BinOpNode>(&mul_side.data);
            if (!mul || mul->op != Op::Mul) return false;

            // Mul children: one must be a Number ∈ {1,2,4,8}, the other a Variable.
            auto extract = [](const TreeNode& a, const TreeNode& b,
                            std::string& idx, long long& sc) -> bool {
                auto* var = std::get_if<Variable>(&a.data);
                auto* num = std::get_if<Number>(&b.data);
                if (!var || !num) return false;
                long long v = num->getValue();
                if (v != 1 && v != 2 && v != 4 && v != 8) return false;
                idx = var->to_string();
                sc  = v;
                return true;
            };

            if (!extract(*mul->left, *mul->right, index_out, scale_out) &&
                !extract(*mul->right, *mul->left, index_out, scale_out)) {
                return false;
            }

            base_out = base_var->to_string();
            return true;
        };

        std::string base, index;
        long long scale = 0;

        if (!try_match(*add->left,  *add->right, base, index, scale) &&
            !try_match(*add->right, *add->left,  base, index, scale)) {
            return "";
        }

        // Full L2 leaq: w @ w w E
        return dest.to_string() + " @ " + base + " " + index + " " + std::to_string(scale);
    }

   static std::vector<std::unique_ptr<Instruction>> emit_instructions(
        const TreeNode& node,
        size_t& fresh_idx)
    {
        std::vector<std::unique_ptr<Instruction>> result;

        // node is a reference — can't be null. Children (unique_ptr) could be,
        // but we'll check those when we dereference them.

        if (auto* binop = std::get_if<BinOpNode>(&node.data)) {
            // let's try leaqmatch before anything first.
            // if it matches, create a rawL2 instruction and push it to the result.

            // if not, and if a child is not a Number or Variable, recursively call
            // emit_instructions on it. May need to look one or two levels down
            // (child / grandchild) to achieve the leaq match.
        }

        return result;
    }

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
                // print_trees(false);
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
        if (instructions.size() != trees.size()) {
            throw std::runtime_error(
                "Context::aggregate_tree: instructions and trees size mismatch (" +
                std::to_string(instructions.size()) + " vs " +
                std::to_string(trees.size()) + ")");
        }

        std::vector<std::unique_ptr<Instruction>> new_instructions;

        for (size_t i = 0; i < instructions.size(); i++) {
            // No tree for this instruction — pass it through unchanged.
            if (trees[i] == nullptr) {
                new_instructions.push_back(std::move(instructions[i]));
                continue;
            }

            // Tree exists — emit instructions from it and append.
            auto emitted = emit_instructions(*trees[i]);
            new_instructions.insert(
                new_instructions.end(),
                std::make_move_iterator(emitted.begin()),
                std::make_move_iterator(emitted.end()));
            }

        instructions = std::move(new_instructions);
    }

} // namespace L3