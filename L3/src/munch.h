


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





    class Tile {
        public:
            virtual ~Tile() = default;
            virtual std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) = 0;
            virtual bool matches(const TreeNode& node) const = 0;
            virtual int64_t cost() const = 0;
    };

    class ShiftTile : public Tile {
        public:
            std::vector<std::unique_ptr<Instruction>> emit(const TreeNode& node) override {
                std::vector<std::unique_ptr<Instruction>> result;


                assert(std::holds_alternative<AssignNode>(node.data));
                const AssignNode& assign = std::get<AssignNode>(node.data);

                assert(std::holds_alternative<BinOpNode>(assign.src->data));
                const BinOpNode& add = std::get<BinOpNode>(assign.src->data);
                assert(add.op == Op::Mul);

                assert(std::holds_alternative<Variable>(assign.dest->data));
                const Variable& dest = std::get<Variable>(assign.dest->data);

                int64_t shift_amount = 0;
                const TreeNode* base_node = nullptr;

                // helper : is x a positive power of 2? If so, set out_log to log2(x)
                auto power_of_two = [](int64_t x, int& out_log) -> bool {
                    if (x <= 0) return false;
                    if ((x & (x - 1)) != 0) return false;
                    int k  = 0;
                    while ((x >>= 1) != 0) k++;
                    out_log = k;
                    return true;
                }

                const TreeNode* cur = assign.src.get();

                while (true) {
                    if (!std::holds_alternative<BinOpNode>(cur->data)){
                        base_node = cur;
                        break;
                    }
                    const BinOpNode& bop = std::get<BinOpNode>(cur.data);
                    if (bop.op != Op::Mul){
                        base_node = cur;
                        break;
                    }


                    const TreeNode* num_side = nullptr;
                    const TreeNode* other_side = nullptr;
                    int k = 0;

                    if (std::holds_alternative<number>(bop.right->data) && power_of_two(std::get<Number>(bop.right->data).getValue(), k)) {
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
                result.push_back(std::make_unique<RawL2Instruction>(shl_str));

                return result;
            }

    }

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
                            + " "   + std::to_string(scale);
                result.push_back(std::make_unique<RawL2Instruction>(s));

                return result;
            }
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
                                std::string instr_str = "\tstore " + std::get<Variable>(left.data).to_string() + " " + std::to_string(std::get<Number>(right.data).getValue());
                                instr_str += " <- ";
                                instr_str += std::visit([](auto&& node) -> std::string {
                                    using T = std::decay_t<decltype(node)>;
                                    if constexpr (std::is_same_v<T, Variable>) return node.to_string();
                                    else if constexpr (std::is_same_v<T, Number>) return node.to_string();
                                    else throw std::runtime_error("store src must be Variable or Number after munching");
                                }, val.data);
                                result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                                return result;
                            }
                        }

                        // store (Number op %a) <- Number/Variable - unlikely but handle it anyway
                        if (std::holds_alternative<Number>(left.data) && std::holds_alternative<Variable>(right.data)){
                            if (std::get<Number>(left.data).getValue() % 8 == 0){
                                std::string instr_str = "\tstore " + std::get<Variable>(right.data).to_string() + " " + std::to_string(std::get<Number>(left.data).getValue());
                                instr_str += " <- ";
                                instr_str += std::visit([](auto&& node) -> std::string {
                                    using T = std::decay_t<decltype(node)>;
                                    if constexpr (std::is_same_v<T, Variable>) return node.to_string();
                                    else if constexpr (std::is_same_v<T, Number>) return node.to_string();
                                    else throw std::runtime_error("store src must be Variable or Number after munching");
                                }, val.data);
                                result.push_back(std::make_unique<RawL2Instruction>(instr_str));
                                return result;
                            }
                        }


                        // store (%a op (Binop)) <- Number/Variable - Tile this Binop separately. 
                        if (std::holds_alternative<Variable>(left.data) && std::holds_alternative<BinOpNode>(right.data)){
                            // 1. Recursively munch the inner BinOp into a temp variable.
                            Variable inner_temp = freshVar();
                            TreeNode synthetic_assign = make_assign_node(inner_temp, *right_tree);
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
            bool matches(const TreeNode& node) const override;
            int64_t cost() const override { return 1; }

    };

}