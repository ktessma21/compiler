


#pragma once

#include <l3.h>

#include <stdio.h>
#include <vector>
#include <string>
#include <cstddef>
#include <memory>
#include <variant>
#include <vector>
#include <set>
#include <type_traits>
#include <stdexcept>
#include <cassert>

#include "ast_leaves.h"  
#include "tree.h" 


namespace L3 {

    Variable freshVar() {
        static int64_t counter = 0;       // persists across calls, fine
        return Variable("newVar" + std::to_string(counter++));
    }

    TreeNode make_assign_node(const Variable& dst, const TreeNode& src) {
        return TreeNode(AssignNode{
            std::make_unique<TreeNode>(dst),
            clone_tree(src)
        });
    }

    std::vector<std::unique_ptr<Instruction>> munch(const TreeNode& node);

    // the recursion, written once, used by munch's ranking
    // int64_t munchCost(const TreeNode& node) {
    //     int64_t best = INT64_MAX;
    //     for (auto& tile : tiles) {
    //         if (!tile->matches(node)) continue;
    //         int64_t c = tile->cost();
    //         // add recursive cost of each operand this tile will force-collapse
    //         for (const TreeNode* child : tile->collapsedOperands(node)) {
    //             c += munchCost(*child);
    //         }
    //         best = std::min(best, c);
    //     }
    //     return best;   // fallback tile guarantees best != INT64_MAX
    // }



    class Tile {
        public:
            virtual ~Tile() = default;
            virtual std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) = 0;
            virtual bool matches(const TreeNode& node) const = 0;
            virtual int64_t cost() const = 0;
    };

   
    class FallbackBinOpTile : public Tile {
        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<BinOpNode>(assign.src->data));
                const BinOpNode& binop = std::get<BinOpNode>(assign.src->data);

                assert(std::holds_alternative<Variable>(assign.dest->data));
                const Variable& dest = std::get<Variable>(assign.dest->data);

                const TreeNode* lhs = binop.left.get();
                const TreeNode* rhs = binop.right.get();

                // Collapse an operand to a leaf operand (T = Variable | Number).
                // Leaves pass through untouched; a BinOp is munched into a fresh temp.
                auto collapse = [&](const TreeNode* operand) -> T {
                    if (std::holds_alternative<Variable>(operand->data)) {
                        return T{std::get<Variable>(operand->data)};
                    } else if (std::holds_alternative<Number>(operand->data)) {
                        return T{std::get<Number>(operand->data)};
                    } else {
                        // sub-expression: munch it into a temp, then use the temp
                        Variable tmp = freshVar();
                        TreeNode synthetic = make_assign_node(tmp, *operand);  
                        auto inner = munch(synthetic);
                        for (auto& ins : inner) result.push_back(std::move(ins));
                        return T{tmp};
                    }
                };

                T left_operand  = collapse(lhs);
                T right_operand = collapse(rhs);

                auto op_instr = std::make_unique<OpInstruction>();
                op_instr->setDst(dest);
                op_instr->setLhs(left_operand);    
                op_instr->setOp(binop.op);
                op_instr->setRhs(right_operand);    
                result.push_back(std::move(op_instr));

                return result;
            }

            bool matches(const TreeNode& node) const override {
                if (!std::holds_alternative<AssignNode>(node.data)) return false;
                const AssignNode& assign = std::get<AssignNode>(node.data);

                if (!assign.dest || !std::holds_alternative<Variable>(assign.dest->data)) return false;
                if (!assign.src  || !std::holds_alternative<BinOpNode>(assign.src->data)) return false;

                // only claim ops your OpInstruction can actually represent
                const BinOpNode& binop = std::get<BinOpNode>(assign.src->data);
                switch (binop.op) {
                    case Op::Add: case Op::Sub: case Op::Mul:
                    case Op::And: case Op::Shl: case Op::Shr:
                        return true;
                    default:
                        return false;
                }
            }

            int64_t cost() const override { return 2; }   // dst <- L ; dst op= R
    };


    // 	%addr <- %base  or 
    // 	%addr <- 9
    class CopyTile : public Tile {
    public:
        std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
            std::vector<std::unique_ptr<Instruction>> result;

            assert(std::holds_alternative<AssignNode>(node.data));
            const AssignNode& assign = std::get<AssignNode>(node.data);
            assert(std::holds_alternative<Variable>(assign.dest->data));
            const Variable& dest = std::get<Variable>(assign.dest->data);

            auto mov = std::make_unique<AssignInstruction>();
            mov->setDst(dest);
            // src is a leaf: Variable or Number (extend with label/@func if your s allows)
            if (std::holds_alternative<Variable>(assign.src->data)) {
                mov->setSrc(std::get<Variable>(assign.src->data));
            } else if (std::holds_alternative<Number>(assign.src->data)) {
                mov->setSrc(std::get<Number>(assign.src->data));
            } else {
                throw std::runtime_error("CopyTile: src not a leaf");
            }
            result.push_back(std::move(mov));
            return result;
        }

        bool matches(const TreeNode& node) const override {
            if (!std::holds_alternative<AssignNode>(node.data)) return false;
            const AssignNode& assign = std::get<AssignNode>(node.data);
            if (!assign.dest || !std::holds_alternative<Variable>(assign.dest->data)) return false;
            if (!assign.src) return false;
            // leaf src only — Variable or Number
            return std::holds_alternative<Variable>(assign.src->data)
                || std::holds_alternative<Number>(assign.src->data);
        }

            int64_t cost() const override { return 1; }
    };


    class ShiftTile : public Tile {
        public:
            int64_t cost() const override { return 2; } 
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;


                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<BinOpNode>(assign.src->data));
                const BinOpNode& add = std::get<BinOpNode>(assign.src->data);
                assert(add.op == Op::Mul);

                assert(std::holds_alternative<Variable>(assign.dest->data));
                const Variable& dest = std::get<Variable>(assign.dest->data);

                

                // helper : is x a positive power of 2? If so, set out_log to log2(x)
                auto power_of_two = [](int64_t x, int& out_log) -> bool {
                    if (x <= 0) return false;
                    if ((x & (x - 1)) != 0) return false;
                    int k  = 0;
                    while ((x >>= 1) != 0) k++;
                    out_log = k;
                    return true;
                };

                int64_t shift_amount = 0;
                const TreeNode* base_node = nullptr;

                const TreeNode* cur = assign.src.get();

                while (true) {
                    if (!std::holds_alternative<BinOpNode>(cur->data)){
                        base_node = cur;
                        break;
                    }
                    const BinOpNode& bop = std::get<BinOpNode>(cur->data);
                    if (bop.op != Op::Mul){
                        base_node = cur;
                        break;
                    }


                    const TreeNode* num_side = nullptr;
                    const TreeNode* other_side = nullptr;
                    int k = 0;

                    if (std::holds_alternative<Number>(bop.right->data) && power_of_two(std::get<Number>(bop.right->data).getValue(), k)) {
                        num_side = bop.right.get();
                        other_side = bop.left.get();
                    } else if (std::holds_alternative<Number>(bop.left->data) && power_of_two(std::get<Number>(bop.left->data).getValue(), k)) {
                        num_side = bop.left.get();
                        other_side = bop.right.get();
                    } else {
                        base_node = cur;
                        break;

                    }

                    shift_amount += k;
                    cur = other_side;

                }

                if (shift_amount == 0){
                    throw std::runtime_error("ShiftTile: not a shift operation");
                }

                 // Resolve the base. If it's already a Variable, use directly.
                // Otherwise, recursively munch it into a fresh temp.
                Variable base_var;
                if (std::holds_alternative<Variable>(base_node->data)) {
                    base_var = std::get<Variable>(base_node->data);
                } else {
                    Variable tmp = freshVar();
                    TreeNode synthetic_assign = make_assign_node(tmp, *base_node);
                    auto inner_instrs = munch(synthetic_assign);
                    for (auto& ins : inner_instrs) result.push_back(std::move(ins));
                    base_var = tmp;
                }

                auto mov = std::make_unique<AssignInstruction>();
                mov->setDst(dest);
                mov->setSrc(base_var);
                result.push_back(std::move(mov));

                std::string shl_str = "\t" + dest.to_string()
                                    + " <<= " + std::to_string(shift_amount);
                shl_str += "\n";
                result.push_back(std::make_unique<RawL2Instruction>(shl_str));

                return result;
            }

        bool matches(const TreeNode& node) const override {
           
            if (!std::holds_alternative<AssignNode>(node.data)) return false;
            const AssignNode& assign = std::get<AssignNode>(node.data);

            // dest must be a Variable (emit does std::get<Variable> on it)
            if (!assign.dest || !std::holds_alternative<Variable>(assign.dest->data)) return false;

            // src must exist and be a BinOp (emit asserts this)
            if (!assign.src || !std::holds_alternative<BinOpNode>(assign.src->data)) return false;

            // same power-of-two test emit uses
            auto power_of_two = [](int64_t x, int& out_log) -> bool {
                if (x <= 0) return false;
                if ((x & (x - 1)) != 0) return false;
                int k = 0;
                while ((x >>= 1) != 0) k++;
                out_log = k;
                return true;
            };

            // walk the Mul chain exactly as emit does, accumulating shift_amount
            int64_t shift_amount = 0;
            const TreeNode* cur = assign.src.get();

            while (true) {
                if (!std::holds_alternative<BinOpNode>(cur->data)) break;       // hit base
                const BinOpNode& bop = std::get<BinOpNode>(cur->data);
                if (bop.op != Op::Mul) break;                                   // hit base

                const TreeNode* other_side = nullptr;
                int k = 0;

                if (std::holds_alternative<Number>(bop.right->data) &&
                    power_of_two(std::get<Number>(bop.right->data).getValue(), k)) {
                    other_side = bop.left.get();
                } else if (std::holds_alternative<Number>(bop.left->data) &&
                        power_of_two(std::get<Number>(bop.left->data).getValue(), k)) {
                    other_side = bop.right.get();
                } else {
                    break;  // no power-of-two factor here -> base
                }

                shift_amount += k;
                cur = other_side;
            }

            // emit throws iff shift_amount == 0, so that's exactly our reject condition
            return shift_amount != 0;
        }

    };

    class LeaqTile : public Tile {
        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<BinOpNode>(assign.src->data));
                const BinOpNode& add = std::get<BinOpNode>(assign.src->data);
                assert(add.op == Op::Add);

                assert(std::holds_alternative<Variable>(assign.dest->data));
                const Variable& dest = std::get<Variable>(assign.dest->data);

                // The Add has two children. One should be the "base" (a Variable),
                // the other should be a Multiply whose operands are (index, scale-const)
                // in some order. We try both orientations.
                const TreeNode* base_node = nullptr;
                const TreeNode* mul_node  = nullptr;

                auto is_mul = [](const TreeNode& n) {
                    return std::holds_alternative<BinOpNode>(n.data)
                        && std::get<BinOpNode>(n.data).op == Op::Mul;
                };

                if (std::holds_alternative<Variable>(add.left->data) && is_mul(*add.right)) {
                    base_node = add.left.get();
                    mul_node  = add.right.get();
                } else if (std::holds_alternative<Variable>(add.right->data) && is_mul(*add.left)) {
                    base_node = add.right.get();
                    mul_node  = add.left.get();
                } else {
                    throw std::runtime_error("LeaqTile: add does not have (Variable, Multiply) shape");
                }

                const Variable& base = std::get<Variable>(base_node->data);
                const BinOpNode& mul = std::get<BinOpNode>(mul_node->data);

                // Within the multiply, one side must be a Number (the scale, ∈ {1,2,4,8}),
                // the other is the index. The index may be a Variable or a sub-BinOp; if
                // it's a sub-BinOp, recursively munch it into a fresh temp.
                const TreeNode* index_node = nullptr;
                const TreeNode* scale_node = nullptr;

                if (std::holds_alternative<Number>(mul.right->data)) {
                    scale_node = mul.right.get();
                    index_node = mul.left.get();
                } else if (std::holds_alternative<Number>(mul.left->data)) {
                    scale_node = mul.left.get();
                    index_node = mul.right.get();
                } else {
                    throw std::runtime_error("LeaqTile: multiply has no constant scale operand");
                }

                int64_t scale = std::get<Number>(scale_node->data).getValue();
                if (scale != 1 && scale != 2 && scale != 4 && scale != 8) {
                    throw std::runtime_error("LeaqTile: scale must be 1, 2, 4, or 8");
                }

                // Resolve the index operand to a Variable. If it's already a Variable,
                // use it directly. Otherwise (it's a BinOp), recursively munch it into
                // a fresh temp and use that temp as the index.
                Variable index_var;
                if (std::holds_alternative<Variable>(index_node->data)) {
                    index_var = std::get<Variable>(index_node->data);
                } else if (std::holds_alternative<BinOpNode>(index_node->data)) {
                    // Build a synthetic AssignNode: index_tmp <- <inner binop>
                    // and munch it, appending its emitted instructions to result.
                    Variable index_tmp = freshVar();
                    TreeNode synthetic_assign = make_assign_node(index_tmp, *index_node);
                    auto inner_instrs = munch(synthetic_assign);
                    for (auto& ins : inner_instrs) result.push_back(std::move(ins));
                    index_var = index_tmp;
                } else {
                    throw std::runtime_error("LeaqTile: index must be Variable or BinOp");
                }

                // Emit the L2 leaq form: dest @ base index scale
                std::string s = "\t" + dest.to_string()
                            + " @ " + base.to_string()
                            + " "   + index_var.to_string()
                            + " "   + std::to_string(scale) + "\n";
                result.push_back(std::make_unique<RawL2Instruction>(s));

                return result;
            }


            bool matches(const TreeNode& node) const override {
                if (!std::holds_alternative<AssignNode>(node.data)) return false;
                const AssignNode& assign = std::get<AssignNode>(node.data);

                if (!assign.dest || !std::holds_alternative<Variable>(assign.dest->data)) return false;
                if (!assign.src  || !std::holds_alternative<BinOpNode>(assign.src->data)) return false;

                const BinOpNode& add = std::get<BinOpNode>(assign.src->data);
                if (add.op != Op::Add) return false;
                if (!add.left || !add.right) return false;

                auto is_mul = [](const TreeNode& n) {
                    return std::holds_alternative<BinOpNode>(n.data)
                        && std::get<BinOpNode>(n.data).op == Op::Mul;
                };

                // (Variable, Mul) in either orientation
                const TreeNode* mul_node = nullptr;
                if (std::holds_alternative<Variable>(add.left->data) && is_mul(*add.right)) {
                    mul_node = add.right.get();
                } else if (std::holds_alternative<Variable>(add.right->data) && is_mul(*add.left)) {
                    mul_node = add.left.get();
                } else {
                    return false;
                }

                // the Mul must have a Number scale on one side, value in {1,2,4,8}
                const BinOpNode& mul = std::get<BinOpNode>(mul_node->data);
                if (!mul.left || !mul.right) return false;

                const TreeNode* scale_node = nullptr;
                if (std::holds_alternative<Number>(mul.right->data)) {
                    scale_node = mul.right.get();
                } else if (std::holds_alternative<Number>(mul.left->data)) {
                    scale_node = mul.left.get();
                } else {
                    return false;
                }

                int64_t scale = std::get<Number>(scale_node->data).getValue();
                return scale == 1 || scale == 2 || scale == 4 || scale == 8;
            }

            int64_t cost() const override { return 1; }   // own footprint: the single @ instruction
        };



    class StoreTile : public Tile {

        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
               
                std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<StoreNode>(assign.dest->data));
                const StoreNode& store = std::get<StoreNode>(assign.dest->data);
                const TreeNode& addr = *store.addr;
                const TreeNode& val = *assign.src;

                if (auto* var = std::get_if<Variable>(&addr.data)){
                    assert(std::holds_alternative<Variable>(val.data) || std::holds_alternative<Number>(val.data));
                    S src_operand = std::visit([](auto&& node) -> S {
                        using T = std::decay_t<decltype(node)>;
                        if constexpr (std::is_same_v<T, Variable>) return S{node};
                        else if constexpr (std::is_same_v<T, Number>) return S{node};
                        else if constexpr (std::is_same_v<T, FunctionName>) return S{node};
                        else throw std::runtime_error("store src must be Variable or Number after munching");
                    }, val.data);

                    auto store = std::make_unique<StoreInstruction>();
                    store->setDst(*var);                    // or whatever your setter is named
                    store->setSrc(std::move(src_operand));
                    result.push_back(std::move(store));
                    return result;
                    
                }

                if (auto* binop = std::get_if<BinOpNode>(&addr.data)){
                    if (binop->op == Op::Add){
                        const TreeNode& left = *binop->left;
                        const TreeNode& right = *binop->right;

                        // store (%a op Number) <- Number/Variable
                        if (std::holds_alternative<Variable>(left.data) && std::holds_alternative<Number>(right.data)){
                            if (std::get<Number>(right.data).getValue() % 8 == 0){
                                std::string instr_str = "\tmem " + std::get<Variable>(left.data).to_string() + " " + std::to_string(std::get<Number>(right.data).getValue());
                                instr_str += " <- ";
                                instr_str += std::visit([](auto&& node) -> std::string {
                                    using T = std::decay_t<decltype(node)>;
                                    if constexpr (std::is_same_v<T, Variable>) return node.to_string();
                                    else if constexpr (std::is_same_v<T, Number>) return node.to_string();
                                     else if constexpr (std::is_same_v<T, FunctionName>) return node.to_string();
                                    else throw std::runtime_error("store src must be Variable or Number after munching");
                                }, val.data);
                                instr_str += "\n";
                                result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                                return result;
                            }
                        }

                        // store (Number op %a) <- Number/Variable - unlikely but handle it anyway
                        if (std::holds_alternative<Number>(left.data) && std::holds_alternative<Variable>(right.data)){
                            if (std::get<Number>(left.data).getValue() % 8 == 0){
                                std::string instr_str = "\tmem " + std::get<Variable>(right.data).to_string() + " " + std::to_string(std::get<Number>(left.data).getValue());
                                instr_str += " <- ";
                                instr_str += std::visit([](auto&& node) -> std::string {
                                    using T = std::decay_t<decltype(node)>;
                                    if constexpr (std::is_same_v<T, Variable>) return node.to_string();
                                    else if constexpr (std::is_same_v<T, Number>) return node.to_string();
                                    else if constexpr (std::is_same_v<T, FunctionName>) return node.to_string();
                                    else throw std::runtime_error("store src must be Variable or Number after munching");
                                }, val.data);
                                instr_str += "\n";
                                result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                                return result;
                            }
                        }


                        // store (%a op (Binop)) <- Number/Variable - Tile this Binop separately. 
                        if (std::holds_alternative<Variable>(left.data) && std::holds_alternative<BinOpNode>(right.data)){
                            // 1. Recursively munch the inner BinOp into a temp variable.
                            Variable inner_temp = freshVar();
                            TreeNode synthetic_assign = make_assign_node(inner_temp, right);
                            auto inner_instrs = munch(synthetic_assign);
                            for (auto& ins : inner_instrs) result.push_back(std::move(ins));

                            Variable base = freshVar();
                            auto add_instr = std::make_unique<OpInstruction>();
                            add_instr->setDst(base);
                            add_instr->setLhs(std::get<Variable>(left.data));
                            add_instr->setOp(Op::Add);          // or whatever your enum value is named
                            add_instr->setRhs(inner_temp);
                            result.push_back(std::move(add_instr));
                      

                            // 3. Emit the store with offset 0, using base as the address.
                            S src_operand = std::visit([](auto&& node) -> S {
                                using T = std::decay_t<decltype(node)>;
                                if constexpr (std::is_same_v<T, Variable>) return S{node};
                                else if constexpr (std::is_same_v<T, Number>) return S{node};
                                else if constexpr (std::is_same_v<T, FunctionName>) return S{node};
                                else throw std::runtime_error("store src must be Variable or Number after munching");
                            }, val.data);

                            auto store_instr = std::make_unique<StoreInstruction>();
                            store_instr->setDst(base);
                            store_instr->setSrc(std::move(src_operand));
                            result.push_back(std::move(store_instr));
                            return result;
                        }



                    }
                }
            
                std::cerr << tree_to_string(node) << "\n";
                throw std::runtime_error("StoreTile failed to match the node structure in emit");

            }

            bool matches(const TreeNode& node) const override {
                if (!std::holds_alternative<AssignNode>(node.data)) return false;
                const AssignNode& assign = std::get<AssignNode>(node.data);

                if (!assign.dest || !std::holds_alternative<StoreNode>(assign.dest->data)) return false;
                if (!assign.src) return false;

                const StoreNode& store = std::get<StoreNode>(assign.dest->data);
                if (!store.addr) return false;
                const TreeNode& addr = *store.addr;

                // case 1: bare Variable address
                if (std::holds_alternative<Variable>(addr.data)) return true;

                // remaining cases require Add
                if (!std::holds_alternative<BinOpNode>(addr.data)) return false;
                const BinOpNode& binop = std::get<BinOpNode>(addr.data);
                if (binop.op != Op::Add) return false;
                if (!binop.left || !binop.right) return false;

                const TreeNode& left  = *binop.left;
                const TreeNode& right = *binop.right;

                // case 2: (Variable + Number), Number % 8 == 0
                if (std::holds_alternative<Variable>(left.data) &&
                    std::holds_alternative<Number>(right.data)) {
                    return std::get<Number>(right.data).getValue() % 8 == 0;
                }

                // case 3: (Number + Variable), Number % 8 == 0
                if (std::holds_alternative<Number>(left.data) &&
                    std::holds_alternative<Variable>(right.data)) {
                    return std::get<Number>(left.data).getValue() % 8 == 0;
                }

                // case 4: (Variable + BinOp) -> emit force-collapses the BinOp
                if (std::holds_alternative<Variable>(left.data) &&
                    std::holds_alternative<BinOpNode>(right.data)) {
                    return true;
                }

                return false;
            }
            int64_t cost() const override { return 1; }

    };

    class CompareTile : public Tile {
        public:
        std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
            std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<Variable>(assign.dest->data));
                const Variable& dest = std::get<Variable>(assign.dest->data);

                assert(std::holds_alternative<CompareNode>(assign.src->data));
                const CompareNode& cmp = std::get<CompareNode>(assign.src->data);


                auto handle_operand = [&](const TreeNode& operand) -> T {
                    if (std::holds_alternative<Variable>(operand.data)) {
                        return T{std::get<Variable>(operand.data)};
                    } else if (std::holds_alternative<Number>(operand.data)){
                        return T{std::get<Number>(operand.data)};
                    } else {
                        Variable tmp = freshVar();
                        TreeNode synthetic_assign = make_assign_node(tmp, operand);
                        auto inner_instrs = munch(synthetic_assign);
                        for (auto& ins : inner_instrs) result.push_back(std::move(ins));
                        return T{tmp};
                    }

                    };



                T left_operand = handle_operand(*cmp.left);
                T right_operand = handle_operand(*cmp.right);

                auto cmp_instr = std::make_unique<CmpInstruction>();
                cmp_instr->setDst(dest);
                cmp_instr->setLhs(left_operand);
                cmp_instr->setCmp(cmp.op);
                cmp_instr->setRhs(right_operand);
                result.push_back(std::move(cmp_instr));
                return result;
                }
            
            bool matches(const TreeNode& node) const override {
                if (!std::holds_alternative<AssignNode>(node.data)) return false;
                const AssignNode& assign = std::get<AssignNode>(node.data);

                if (!assign.dest || !std::holds_alternative<Variable>(assign.dest->data)) return false;
                if (!assign.src  || !std::holds_alternative<CompareNode>(assign.src->data))  return false;


                const CompareNode& cmp = std::get<CompareNode>(assign.src->data);
                if (!cmp.left || !cmp.right) return false;
                return true;
       
            }

            int64_t cost() const override {return 2;}


    };
    


    class LoadTile : public Tile {
        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<Variable>(assign.dest->data));
                const Variable& dest = std::get<Variable>(assign.dest->data);

                assert(std::holds_alternative<LoadNode>(assign.src->data));
                const LoadNode& load = std::get<LoadNode>(assign.src->data);
                const TreeNode& addr = *load.addr;

                // case 1: bare Variable address -> dst <- load var  (offset 0)
                if (auto* var = std::get_if<Variable>(&addr.data)) {
                    auto load_instr = std::make_unique<LoadInstruction>();
                    load_instr->setDst(dest);
                    load_instr->setSrc(*var);
                    result.push_back(std::move(load_instr));
                    return result;
                }

                if (auto* binop = std::get_if<BinOpNode>(&addr.data)) {
                    if (binop->op == Op::Add) {
                        const TreeNode& left  = *binop->left;
                        const TreeNode& right = *binop->right;

                        // case 2: (Variable + Number), Number % 8 == 0  ->  dst <- mem var const
                        if (std::holds_alternative<Variable>(left.data) &&
                            std::holds_alternative<Number>(right.data)) {
                            if (std::get<Number>(right.data).getValue() % 8 == 0) {
                                std::string instr_str = "\t" + dest.to_string() + " <- mem "
                                    + std::get<Variable>(left.data).to_string() + " "
                                    + std::to_string(std::get<Number>(right.data).getValue())
                                    + "\n";
                                result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                                return result;
                            }
                        }

                        // case 3: (Number + Variable), Number % 8 == 0  ->  dst <- mem var const
                        if (std::holds_alternative<Number>(left.data) &&
                            std::holds_alternative<Variable>(right.data)) {
                            if (std::get<Number>(left.data).getValue() % 8 == 0) {
                                std::string instr_str = "\t" + dest.to_string() + " <- mem "
                                    + std::get<Variable>(right.data).to_string() + " "
                                    + std::to_string(std::get<Number>(left.data).getValue())
                                    + "\n";
                                result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                                return result;
                            }
                        }

                        // case 4: (Variable + BinOp) -> collapse the inner binop, add to base, load offset 0
                        if (std::holds_alternative<Variable>(left.data) &&
                            std::holds_alternative<BinOpNode>(right.data)) {
                            Variable inner_temp = freshVar();
                            TreeNode synthetic_assign = make_assign_node(inner_temp, right);
                            auto inner_instrs = munch(synthetic_assign);
                            for (auto& ins : inner_instrs) result.push_back(std::move(ins));

                            Variable base = freshVar();
                            auto add_instr = std::make_unique<OpInstruction>();
                            add_instr->setDst(base);
                            add_instr->setLhs(std::get<Variable>(left.data));
                            add_instr->setOp(Op::Add);
                            add_instr->setRhs(inner_temp);
                            result.push_back(std::move(add_instr));

                            auto load_instr = std::make_unique<LoadInstruction>();
                            load_instr->setDst(dest);
                            load_instr->setSrc(base);
                            result.push_back(std::move(load_instr));
                            return result;
                        }
                    }
                }

                std::cerr << tree_to_string(node) << "\n";
                throw std::runtime_error("LoadTile failed to match the node structure in emit");
            }

            bool matches(const TreeNode& node) const override {
                if (!std::holds_alternative<AssignNode>(node.data)) return false;
                const AssignNode& assign = std::get<AssignNode>(node.data);

                if (!assign.dest || !std::holds_alternative<Variable>(assign.dest->data)) return false;
                if (!assign.src  || !std::holds_alternative<LoadNode>(assign.src->data))  return false;

                const LoadNode& load = std::get<LoadNode>(assign.src->data);
                if (!load.addr) return false;
                const TreeNode& addr = *load.addr;

                // case 1: bare Variable
                if (std::holds_alternative<Variable>(addr.data)) return true;

                // remaining cases require Add
                if (!std::holds_alternative<BinOpNode>(addr.data)) return false;
                const BinOpNode& binop = std::get<BinOpNode>(addr.data);
                if (binop.op != Op::Add) return false;
                if (!binop.left || !binop.right) return false;

                const TreeNode& left  = *binop.left;
                const TreeNode& right = *binop.right;

                // case 2: (Variable + Number), Number % 8 == 0
                if (std::holds_alternative<Variable>(left.data) &&
                    std::holds_alternative<Number>(right.data)) {
                    return std::get<Number>(right.data).getValue() % 8 == 0;
                }

                // case 3: (Number + Variable), Number % 8 == 0
                if (std::holds_alternative<Number>(left.data) &&
                    std::holds_alternative<Variable>(right.data)) {
                    return std::get<Number>(left.data).getValue() % 8 == 0;
                }

                // case 4: (Variable + BinOp)
                if (std::holds_alternative<Variable>(left.data) &&
                    std::holds_alternative<BinOpNode>(right.data)) {
                    return true;
                }

                return false;
            }

            int64_t cost() const override { return 1; }
    };


    class CallTile : public Tile {
        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<CallNode>(assign.src->data));
                const CallNode& call = std::get<CallNode>(assign.src->data);

                assert(std::holds_alternative<Variable>(assign.dest->data));
                const Variable& dest = std::get<Variable>(assign.dest->data);
                auto call_instr = std::make_unique<VarCallInstruction>();
                call_instr->setDst(dest);
                call_instr->setCallee(call.callee);

                for (auto& arg : call.args) {
                    if (std::holds_alternative<Variable>(arg->data)) {
                        call_instr->addArg(T{std::get<Variable>(arg->data)});
                    } else if (std::holds_alternative<Number>(arg->data)) {
                        call_instr->addArg(T{std::get<Number>(arg->data)});
                    } else {
                        Variable arg_tmp = freshVar();
                        TreeNode synthetic_assign = make_assign_node(arg_tmp, *arg);
                        auto inner_instrs = munch(synthetic_assign);
                        for (auto& ins : inner_instrs) result.push_back(std::move(ins));
                        call_instr->addArg(T{arg_tmp});
                    }
                }

                result.push_back(std::move(call_instr));
                return result;
            }

            bool matches(const TreeNode& node) const override {
                if (!std::holds_alternative<AssignNode>(node.data)) return false;
                const AssignNode& assign = std::get<AssignNode>(node.data);

                if (!assign.dest || !std::holds_alternative<Variable>(assign.dest->data)) return false;
                if (!assign.src  || !std::holds_alternative<CallNode>(assign.src->data))  return false;

                return true;
            }

            int64_t cost() const override { return 1; }
    };

    class VoidCallTile : public Tile {
        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<CallNode>(node.data));
                const CallNode& call = std::get<CallNode>(node.data);

        
                auto call_instr = std::make_unique<CallInstruction>();
                call_instr->setCallee(call.callee);

                for (auto& arg : call.args) {
                    if (std::holds_alternative<Variable>(arg->data)) {
                        call_instr->addArg(T{std::get<Variable>(arg->data)});
                    } else if (std::holds_alternative<Number>(arg->data)) {
                        call_instr->addArg(T{std::get<Number>(arg->data)});
                    } else {
                        Variable arg_tmp = freshVar();
                        TreeNode synthetic_assign = make_assign_node(arg_tmp, *arg);
                        auto inner_instrs = munch(synthetic_assign);
                        for (auto& ins : inner_instrs) result.push_back(std::move(ins));
                        call_instr->addArg(T{arg_tmp});
                    }
                }

                result.push_back(std::move(call_instr));
                return result;
            }

            bool matches(const TreeNode& node) const override {
                if (!std::holds_alternative<CallNode>(node.data)) return false;
                
                return true;
            }

            int64_t cost() const override { return 1; }
    };


     class ReturnTile : public Tile {
        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;

                assert(std::holds_alternative<ReturnNode>(node.data));
                const ReturnNode& ret = std::get<ReturnNode>(node.data);

                auto return_instr = std::make_unique<ReturnTInstruction>();

                const TreeNode& val = *ret.value;

                if (std::holds_alternative<Variable>(val.data)) {
                    return_instr->setValue(T{std::get<Variable>(val.data)});
                }else if (std::holds_alternative<Number>(val.data)) {
                    return_instr->setValue(T{std::get<Number>(val.data)});
                } else {
                    Variable ret_tmp = freshVar();
                    TreeNode synthetic_assign = make_assign_node(ret_tmp, val);
                    auto inner_instrs = munch(synthetic_assign);
                    for (auto& ins : inner_instrs) result.push_back(std::move(ins));
                    return_instr->setValue(T{ret_tmp});
                }
                result.push_back(std::move(return_instr));
                return result;
            }

                bool matches(const TreeNode& node) const override {
                    if (!std::holds_alternative<ReturnNode>(node.data)) return false;
                    return true;
                }

                int64_t cost() const override { return 1; }
        };

        class BrTile : public Tile {
            public:
                std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                    std::vector<std::unique_ptr<Instruction>> result;

                    assert(std::holds_alternative<BrNode>(node.data));
                    const BrNode& br = std::get<BrNode>(node.data);

                    auto br_instr = std::make_unique<BrTInstruction>();
                    br_instr->setTarget(Label(br.true_label));

                    const TreeNode& cond = *br.cond;

                    if (std::holds_alternative<Variable>(cond.data)) {
                        br_instr->setCond(T{std::get<Variable>(cond.data)});
                    } else if (std::holds_alternative<Number>(cond.data)) {
                        br_instr->setCond(T{std::get<Number>(cond.data)});
                    } else {
                        Variable cond_tmp = freshVar();
                        TreeNode synthetic_assign = make_assign_node(cond_tmp, cond);
                        auto inner_instrs = munch(synthetic_assign);
                        for (auto& ins : inner_instrs) result.push_back(std::move(ins));
                        br_instr->setCond(T{cond_tmp});
                    }
                    result.push_back(std::move(br_instr));
                    return result;
                }

                bool matches(const TreeNode& node) const override {
                    if (!std::holds_alternative<BrNode>(node.data)) return false;
                    return true;
                }

                int64_t cost() const override { return 1; }
        };







    std::vector<std::unique_ptr<Instruction>> munch(const TreeNode& node) {
        // Build the tile set. For now, the specialized tiles you have.
        // (Eventually add the fallback/atomic tile as the guaranteed floor.)
        std::vector<std::unique_ptr<Tile>> tiles;
        tiles.push_back(std::make_unique<ShiftTile>());
        tiles.push_back(std::make_unique<LeaqTile>());
        tiles.push_back(std::make_unique<StoreTile>());
        tiles.push_back(std::make_unique<CallTile>());
        tiles.push_back(std::make_unique<FallbackBinOpTile>());
        tiles.push_back(std::make_unique<CopyTile>());
        tiles.push_back(std::make_unique<LoadTile>());
        tiles.push_back(std::make_unique<VoidCallTile>());
        tiles.push_back(std::make_unique<ReturnTile>());
        tiles.push_back(std::make_unique<BrTile>());
        tiles.push_back(std::make_unique<CompareTile>());
        // tiles.push_back(std::make_unique<FallbackTile>());  // add when ready

        Tile* best = nullptr;
        for (auto& t : tiles) {
            if (!t->matches(node)) continue;
            if (best == nullptr || t->cost() < best->cost()) {
                best = t.get();
            }
        }

        if (best == nullptr) {
            std::cerr << tree_to_string(node) << "\n";
            throw std::runtime_error("munch: no tile matched this node");
        }

        return best->emit(node);
    }

}