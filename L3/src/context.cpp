// context.cpp

#include "l3.h"  

#include "context.h" 
#include "tree.h"


namespace L3 {


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

   


    static std::vector<std::unique_ptr<Instruction>> emit_instructions(
        const TreeNode& node,
        size_t& fresh_idx)
    {

        // At the very top of emit_instructions, before anything else:
        // std::cerr << "[emit_instructions] top-level node: "
        //         << tree_to_string(node) << "\n";
        // std::cerr << "[emit_instructions] variant index: "
        //         << node.data.index() << "\n";

// define
// :entry
// a @ b c
// return a
        std::vector<std::unique_ptr<Instruction>> result;

        // Helper: lift any non-Variable subtree into a fresh temp.
        auto lift_to_var = [&](const TreeNode& child) -> Variable {
            if (auto* v = std::get_if<Variable>(&child.data)) return *v;

            Variable tmp{"emit_tmp_" + std::to_string(fresh_idx++)};
            auto tmp_dest  = std::make_unique<TreeNode>(tmp);
            auto child_cln = clone_tree(child);
            TreeNode sub_assign{AssignNode{std::move(tmp_dest), std::move(child_cln)}};

            auto sub_instrs = emit_instructions(sub_assign, fresh_idx);
            result.insert(result.end(),
                        std::make_move_iterator(sub_instrs.begin()),
                        std::make_move_iterator(sub_instrs.end()));
            return tmp;
        };

        // ---- Top-level ReturnNode: `return` or `return t` ----
        if (auto* ret = std::get_if<ReturnNode>(&node.data)) {
            if (!ret->value) {
                result.push_back(std::make_unique<ReturnInstruction>());
                return result;
            }

            // return t — t must be Variable or Number; lift if not.
            const TreeNode& v = *ret->value;
            T val_t = std::visit([&](const auto& x) -> T {
                using X = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<X, Variable> || std::is_same_v<X, Number>) {
                    return T{x};
                } else {
                    return T{lift_to_var(v)};
                }
            }, v.data);

            auto instr = std::make_unique<ReturnTInstruction>();
            instr->setValue(val_t);
            result.push_back(std::move(instr));
            return result;
        }

        // ---- Top-level bare CallNode (void call): `call callee(args)` ----
        if (auto* call = std::get_if<CallNode>(&node.data)) {
            std::vector<T> arg_ts;
            arg_ts.reserve(call->args.size());
            for (const auto& arg : call->args) {
                const TreeNode& a = *arg;
                if (auto* v = std::get_if<Variable>(&a.data))      arg_ts.push_back(T{*v});
                else if (auto* n = std::get_if<Number>(&a.data))   arg_ts.push_back(T{*n});
                else                                               arg_ts.push_back(T{lift_to_var(a)});
            }

            auto instr = std::make_unique<CallInstruction>();
            instr->setCallee(call->callee);
            for (auto& t : arg_ts) instr->addArg(std::move(t));
            result.push_back(std::move(instr));
            return result;
        }

        // ---- Top-level branch condition: bare Variable or Number ----
        // Nothing to emit; the BrT itself was passed through unchanged.
        if (std::holds_alternative<Variable>(node.data) ||
            std::holds_alternative<Number>(node.data)) {
            return result;
        }

        // ---- Everything below requires an AssignNode at the top ----
        auto* assign = std::get_if<AssignNode>(&node.data);
        if (!assign) {
            throw std::runtime_error("emit_instructions: expected AssignNode at top");
        }

        // std::cerr << "[emit_instructions] AssignNode dest variant: "
        //         << assign->dest->data.index()
        //         << "  src variant: " << assign->src->data.index() << "\n";

        if (auto* store_dest = std::get_if<StoreNode>(&assign->dest->data)) {
            const TreeNode& addr_var = *store_dest->addr;
            const TreeNode& val = *assign->src;

            // Try to fold `base + N` (N a multiple of 8) into `mem base N`
            if (!isNestedBinop(addr_var)) {
                if (auto* bin = std::get_if<BinOpNode>(&addr_var.data)) {
                    auto* num  = std::get_if<Number>(&bin->right->data);
                    auto* base = std::get_if<Variable>(&bin->left->data);
                    if (!num) {
                        num  = std::get_if<Number>(&bin->left->data);
                        base = std::get_if<Variable>(&bin->right->data);
                    }
                    if (num && base && num->getValue() % 8 == 0) {
                        auto& val_num = std::get<Number>(val.data);  // ASSUMPTION: comeback for now
                        std::string instr_str =
                            "\tmem " + base->to_string() + " " +
                            std::to_string(num->getValue()) + " <- " +
                            val_num.to_string() + "\n";
                        result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                        return result;
                    }
                } else if (auto* var = std::get_if<Variable>(&addr_var.data)){ // clear case: just a variable address, and no folding. 
                    auto& val_num = std::get<Number>(val.data);  // ASSUMPTION: comeback for now
                    std::string instr_str =
                        "\tmem " + var->to_string() + " <- " +
                        val_num.to_string() + "\n";
                    result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                    return result;
                }
            }

            // Fallback: lift the address to a variable if it's nested, otherwise use it directly
            Variable addr_var_out = std::visit([&](const auto& v) -> Variable {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, Variable>) {
                    return v;
                } else {
                    return lift_to_var(addr_var);
                }
            }, addr_var.data);

            S val_s = std::visit([&](const auto& v) -> S {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, Variable> || std::is_same_v<V, Number>) {
                    return S{v};
                } else {
                    return S{lift_to_var(val)};
                }
            }, val.data);

            auto instr = std::make_unique<StoreInstruction>();
            instr->setDst(addr_var_out);
            instr->setSrc(val_s);
            result.push_back(std::move(instr));
            return result;
        }

        // ---- Case B: dest is a Variable  →  normal assignment ----
        auto* dest_var = std::get_if<Variable>(&assign->dest->data);
        if (!dest_var) {
            throw std::runtime_error("emit_instructions: AssignNode dest must be Variable or StoreNode");
        }

        const TreeNode& src = *assign->src;

        // B1: dest <- Number
        if (auto* num = std::get_if<Number>(&src.data)) {
            auto instr = std::make_unique<AssignInstruction>();
            instr->setDst(*dest_var);
            instr->setSrc(S{*num});
            result.push_back(std::move(instr));
            return result;
        }

        // B2: dest <- Variable
        if (auto* var = std::get_if<Variable>(&src.data)) {
            auto instr = std::make_unique<AssignInstruction>();
            instr->setDst(*dest_var);
            instr->setSrc(S{*var});
            result.push_back(std::move(instr));
            return result;
        }

        // B3: dest <- BinOp(...)
        if (auto* binop = std::get_if<BinOpNode>(&src.data)) {
            // Try leaq first.
            std::string leaq = leaqMatch(*dest_var, src);
            if (!leaq.empty()) {
                throw std::runtime_error("worked");
                result.push_back(std::make_unique<RawL2Instruction>(leaq));
                return result;
            }

            auto lift_to_t = [&](const TreeNode& child) -> T {
                if (auto* v = std::get_if<Variable>(&child.data)) return T{*v};
                if (auto* n = std::get_if<Number>(&child.data))   return T{*n};
                return T{lift_to_var(child)};
            };

            T lhs_t = lift_to_t(*binop->left);
            T rhs_t = lift_to_t(*binop->right);

            auto instr = std::make_unique<OpInstruction>();
            instr->setDst(*dest_var);
            instr->setLhs(lhs_t);
            instr->setOp(binop->op);
            instr->setRhs(rhs_t);
            result.push_back(std::move(instr));
            return result;
        }

        // B4: dest <- load(addr)
        if (auto* load = std::get_if<LoadNode>(&src.data)) {
            const TreeNode& addr = *load->addr;
            // Try to tile: load(BinOp(Add, Var, Number M)) where M % 8 == 0
            if (auto* binop = std::get_if<BinOpNode>(&addr.data); binop && binop->op == Op::Add) {
                auto try_tile = [&](const TreeNode& var_side, const TreeNode& num_side) -> bool {
                    auto* base = std::get_if<Variable>(&var_side.data);
                    auto* num  = std::get_if<Number>(&num_side.data);
                    if (!base || !num) return false;
                    long long off = num->getValue();
                    if (off % 8 != 0) return false;

                    std::string instr_str = "\t" + dest_var->to_string() + " <- mem " +
                                            base->to_string() + " " +
                                            std::to_string(off) + "\n";
                    result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                    return true;
                };

                if (try_tile(*binop->left, *binop->right)) return result;
                if (try_tile(*binop->right, *binop->left)) return result;
            }


            Variable addr_var = lift_to_var(*load->addr);
            auto instr = std::make_unique<LoadInstruction>();
            instr->setDst(*dest_var);
            instr->setSrc(addr_var);
            result.push_back(std::move(instr));
            return result;
        }

        // B5: dest <- Compare(...)
        if (auto* cmp = std::get_if<CompareNode>(&src.data)) {
            auto lift_to_t = [&](const TreeNode& child) -> T {
                if (auto* v = std::get_if<Variable>(&child.data)) return T{*v};
                if (auto* n = std::get_if<Number>(&child.data))   return T{*n};
                return T{lift_to_var(child)};
            };

            T lhs_t = lift_to_t(*cmp->left);
            T rhs_t = lift_to_t(*cmp->right);

            auto instr = std::make_unique<CmpInstruction>();
            instr->setDst(*dest_var);
            instr->setLhs(lhs_t);
            instr->setCmp(cmp->op);
            instr->setRhs(rhs_t);
            result.push_back(std::move(instr));
            return result;
        }

        // B6: dest <- call callee(args...)
        if (auto* call = std::get_if<CallNode>(&src.data)) {
            std::vector<T> arg_ts;
            arg_ts.reserve(call->args.size());
            for (const auto& arg : call->args) {
                const TreeNode& a = *arg;
                if (auto* v = std::get_if<Variable>(&a.data))      arg_ts.push_back(T{*v});
                else if (auto* n = std::get_if<Number>(&a.data))   arg_ts.push_back(T{*n});
                else                                               arg_ts.push_back(T{lift_to_var(a)});
            }

            auto instr = std::make_unique<VarCallInstruction>();
            instr->setDst(*dest_var);
            instr->setCallee(call->callee);
            for (auto& t : arg_ts) instr->addArg(std::move(t));
            result.push_back(std::move(instr));
            return result;
        }

        return result;
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

                bool any_reader_seen = false;
                bool all_readers_merged = true;

                for (int j = i + 1; j <= death; j++) {
                    bool is_redef = trees[j]
                        ? tree_writes(*trees[j]).count(defined)
                        : instructions[j]->writes().count(defined);

                    bool j_reads = trees[j]
                        ? tree_reads(*trees[j]).count(defined)
                        : instructions[j]->reads().count(defined);

                    if (j_reads) {
                        any_reader_seen = true;
                        auto source_copy = clone_tree(*trees[i]);
                        auto target_copy = clone_tree(*trees[j]);
                        auto merged = L3::merge_tree(std::move(source_copy), std::move(target_copy));
                        if (merged) {
                            trees[j] = std::move(merged);
                        } else {
                            all_readers_merged = false;
                            break;
                        }
                    }
                    if (is_redef) break;
                }

                if (any_reader_seen && all_readers_merged) {
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
            if (t) {
                // std::cerr << "[shrink loop] before:\n";
                // print_trees(true);
                t = shrink_tree(*t);
                // std::cerr << "[shrink loop] after:\n";
                // print_trees(true);
            }
        }

        // combine or merge the last two if they are mergeable 
        print_trees(false);
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

            auto emitted = emit_instructions(*trees[i], fresh_idx);
            new_instructions.insert(
                new_instructions.end(),
                std::make_move_iterator(emitted.begin()),
                std::make_move_iterator(emitted.end()));
            }

        instructions = std::move(new_instructions);
    }

} // namespace L3