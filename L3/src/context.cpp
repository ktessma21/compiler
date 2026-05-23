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

    static bool isNestedBinop(const TreeNode& node) {
        if (auto* binop = std::get_if<BinOpNode>(&node.data)){
            return std::holds_alternative<BinOpNode>(binop->left->data) ||
                    std::holds_alternative<BinOpNode>(binop->right->data);
        }
        return false;
    }


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

                    
                    // ---------- KEY DIAGNOSTIC ----------
                bool other_reader = false;
                for (size_t t = 0; t < trees.size(); t++) {
                    if (t == i || t == j) continue;
                    if (!trees[t]) continue;
                    if (tree_reads(*trees[t]).count(defined)) { other_reader = true; break; }
                }
                if (!dead_after_j || other_reader) {
                    continue;   // merge into j but do NOT delete the source
                }

                    // Try to merge tree[i] into tree[j]
                //   // count how many times `defined` appears as a leaf in target BEFORE merge
                // int leaf_count_before = count_var_leaves(*trees[j], defined);

                try {
                    auto source_copy = trees[i] ? clone_tree(*trees[i]) : nullptr;
                    auto target_copy = trees[j] ? clone_tree(*trees[j]) : nullptr;
                    auto merged = L3::merge_tree(std::move(source_copy), std::move(target_copy));

                    // // ---------- ORPHAN-LEAF DETECTOR ----------
                    // // After merging, `defined` should NOT survive as a leaf in `merged`,
                    // // because we are about to delete its sole definition (tree[i]).
                    // int leaf_count_after = count_var_leaves(*merged, defined);
                    // if (leaf_count_after > 0) {
                    //     std::cerr << "  *** ORPHAN WARNING *** after merging def="
                    //               << defined.toString()
                    //               << " (i=" << i << " into j=" << j << "), the variable still "
                    //               << "appears as " << leaf_count_after
                    //               << " leaf(s) in the merged tree, but tree[" << i
                    //               << "] (its definition) is about to be erased.\n";
                    //     std::cerr << "      leaves before=" << leaf_count_before
                    //               << " leaves after=" << leaf_count_after << "\n";
                    //     std::cerr << "      merged = " << tree_to_string(*merged) << "\n";
                    // }

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
            new_instructions.insert(
                new_instructions.end(),
                std::make_move_iterator(emitted.begin()),
                std::make_move_iterator(emitted.end()));
            }

        instructions = std::move(new_instructions);
    }

} // namespace L3