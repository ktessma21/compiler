


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
                    }
                }
            

                return result;

            }
            bool matches(const TreeNode& node) const override;
            int64_t cost() const override { return 1; }

    };

}