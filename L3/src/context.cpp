// context.cpp

#include "l3.h"  

#include "context.h" 
#include "tree.h"
#include "munch.h"
#include <cstdlib>


namespace L3 {


    inline bool debug_enabled() {
        static bool v = (std::getenv("L3_DEBUG") != nullptr);
        return v;
    }

    

    /// Context function definitions. 

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
            || t == InstructionType::AssignFromCall
            || t == InstructionType::Call;
    }

    void Context::print_trees(bool debug) const {
        if (!debug_enabled()) return;

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


    inline bool tree_has_memory_op(const TreeNode& node) {
    return std::visit([](const auto& data) -> bool {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, LoadNode> || std::is_same_v<T, StoreNode>) {
            return true;
        } else if constexpr (std::is_same_v<T, Variable> || std::is_same_v<T, Number>) {
            return false;
        } else if constexpr (std::is_same_v<T, BinOpNode> || std::is_same_v<T, CompareNode>) {
            return tree_has_memory_op(*data.left) || tree_has_memory_op(*data.right);
        } else if constexpr (std::is_same_v<T, AssignNode>) {
            return tree_has_memory_op(*data.dest) || tree_has_memory_op(*data.src);
        } else if constexpr (std::is_same_v<T, ReturnNode>) {
            return data.value && tree_has_memory_op(*data.value);
        } else if constexpr (std::is_same_v<T, CallNode>) {
            // Calls can read/write arbitrary memory — treat them as memory ops too
            return true;
        } else if constexpr (std::is_same_v<T, BrNode>) {
            return data.cond && tree_has_memory_op(*data.cond);
        }
        return false;
    }, node.data);
}




    void Context::merge_tree() {

        if (std::getenv("L3_NO_MERGE")) {
        // skip all merging; just build trees 1:1 and let munch lower them
        print_trees(true);
        return;
    }
        print_trees(true);
 
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

        int pass = 0;
        bool changed = true;
        while (changed) {
            changed = false;
            
            // recompute liveness at start of every pass for each context. 
            liveAnalysisReport = compute_liveness(*this);

            // std::cerr << "\n========== MERGE PASS " << pass++
            //       << "  (trees=" << trees.size() << ") ==========\n";


            for (size_t i = 0; i < trees.size(); i++) {
                // print_trees(false);
                if (!trees[i]) continue;

                // find if there is anything to merge 
                auto written = tree_writes(*trees[i]);
                if (written.empty()) continue;          // store / return / void call
                assert(written.size() == 1);            // your trees define at most one var
            
  
                // `defined` is the Variable written by tree[i]. It corresponds to:
                    //   - AssignNode { dest: Variable, src: ... }  → defined = the Variable in dest
                    //   - StoreNode trees:  dest = StoreNode (writes memory, no variable)  → skipped (diff empty)
                    //   - ReturnNode / void CallNode: no dest at all  → skipped (diff empty)
                const Variable& defined = *written.begin();


                /*
                - If tree j redefines defined, it stops — you can't merge across a redefinition (the value would be stale).
                - If tree j doesn't read defined, it keeps scanning.
                - If tree j reads defined, it becomes the merge target candidate, subject to safety checks.
                */

                for (size_t j = i + 1; j < trees.size(); j++) {
                    // if j redefines, stop looking further - cannot merge past a redefinition 
                    bool j_redefs = trees[j]
                        ? tree_writes(*trees[j]).count(defined)
                        : instructions[j]->writes().count(defined);

                    if (j_redefs) {
                        // std::cerr << "  [i=" << i << " def=" << defined.to_string()
                        //       << "] STOP: redefined at j=" << j << "\n";
                    
                        break;
                    }

                    // if j reads, try to merge tree[i] into tree[j]. 
                    bool j_reads = trees[j]
                        ? tree_reads(*trees[j]).count(defined)
                        : instructions[j]->reads().count(defined);


                    if (!j_reads) continue;


                    // the reads from i should stay not overwritten before j. 
                    auto source_reads = tree_reads(*trees[i]);
                    bool input_clobbered = false;
                    for (size_t k = i + 1; k < j; k++) {
                        std::set<Variable> k_writes = trees[k] ? tree_writes(*trees[k])
                                                            : instructions[k]->writes();
                        for (const auto& w : k_writes)
                            if (source_reads.count(w)) { input_clobbered = true; break; }
                        if (input_clobbered) break;
                    }
                    if (input_clobbered) continue;



                    // Alias-safety check: if either source or target touches memory,
                    // make sure nothing between them touches memory either.
                    bool source_has_mem = trees[i] ? tree_has_memory_op(*trees[i]) : false;
                    bool target_has_mem = trees[j] ? tree_has_memory_op(*trees[j]) : false;
                    
                    if (source_has_mem || target_has_mem) {
                        bool path_clear = true;
                        for (size_t k = i + 1; k < j; k++) {
                            if (!trees[k]) continue;  // labels etc. don't touch memory
                            if (tree_has_memory_op(*trees[k])) {
                                path_clear = false;
                                break;
                            }
                        }
                        if (!path_clear) {
                            // std::cerr << "  [i=" << i << " -> j=" << j
                            //         << "] SKIP: memory op between\n";
                            continue;
                        }
                    }

                bool dead_after_j = (j < liveAnalysisReport.size())
                    && (liveAnalysisReport[j].out.count(defined) == 0);

                    
                    //
                bool other_reader = false;
                for (size_t t = 0; t < trees.size(); t++) {
                    if (t == i || t == j) continue;
                    if (!trees[t]) continue;
                    if (tree_reads(*trees[t]).count(defined)) { other_reader = true; break; }
                }


                // A variable live at the block's exit escapes to a successor block.
                // Deleting its definition orphans that cross-block use, so refuse.
                bool live_out_of_block = false;
                if (!liveAnalysisReport.empty()) {
                    const std::set<Variable>& exit_live = liveAnalysisReport.back().out;
                    live_out_of_block = (exit_live.count(defined) != 0);
                }

                if (!dead_after_j || other_reader || live_out_of_block) {
                    continue;
                }

                    // Try to merge tree[i] into tree[j]
                //   // count how many times `defined` appears as a leaf in target BEFORE merge
                // int leaf_count_before = count_var_leaves(*trees[j], defined);

                try {
                    auto source_copy = trees[i] ? clone_tree(*trees[i]) : nullptr;
                    auto target_copy = trees[j] ? clone_tree(*trees[j]) : nullptr;
                    auto merged = L3::merge_tree(std::move(source_copy), std::move(target_copy));

                    if (!merged) {
                        continue;   // merge helper said "not a match" — leave both slots untouched
                    }

                    // Inspect the candidate WITHOUT committing it yet.
                    if (tree_reads(*merged).count(defined)) {
                        // The merge didn't fully consume `defined` — a leaf survived.
                        // Do NOT commit and do NOT erase the source. Leave everything as-is.
                        continue;
                    }

                    trees[j] = std::move(merged);
                    
                    // std::cerr << "      MERGED -> tree[" << j << "] now = "
                    //           << tree_to_string(*trees[j]) << "\n";
                } catch (const std::exception& e) {
                    std::cerr << " merge threw: " << e.what() << "\n";
                    throw;
                }




                    instructions.erase(instructions.begin() + i);
                    trees.erase(trees.begin() + i);
                    liveAnalysisReport.erase(liveAnalysisReport.begin() + i);
                    i--;
                    changed = true;
                   
                }


            }
        }

   
        // std::cerr << "=== after merge_tree ===\n";
        // // combine or merge the last two if they are mergeable 
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
        size_t fresh_idx = 0;

        for (size_t i = 0; i < instructions.size(); i++) {
        
            if (trees[i] == nullptr) {
                new_instructions.push_back(std::move(instructions[i]));
                continue;
            }

            auto emitted = munch(*trees[i]);
            if (debug_enabled()) {
                std::cerr << "MUNCH tree[" << i << "] = " << tree_to_string(*trees[i]) << "\n";
                for (auto& e : emitted) std::cerr << "      EMIT: " << e->to_string();
            }

            new_instructions.insert(
                new_instructions.end(),
                std::make_move_iterator(emitted.begin()),
                std::make_move_iterator(emitted.end()));
            }


            
        instructions = std::move(new_instructions);
    }

} // namespace L3