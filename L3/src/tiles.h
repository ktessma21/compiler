// tiles.h
#pragma once
#include <memory>
#include <tree.h>
#include <ast_leaves.h>

namespace L3 {
    class Instruction;
    struct TreeNode;

    struct Tile {
        virtual ~Tile() = default;
        virtual int cost() const = 0;
        virtual bool match(const TreeNode& node) const = 0;
        virtual std::unique_ptr<Instruction> emit(const TreeNode& node) const = 0;
    };

    struct StoreTile : Tile {
        int cost() const override {  
            return 1;
        }

        bool match(const TreeNode& node) const override {
            auto* store = std::get_if<StoreNode>(&node.data);
            if (!store) return false;
            auto* binop = std::get_if<BinOpNode>(&store->addr->data);
            if (!binop) return false;
            // only + and - make sense as address offsets
            return (binop->op == Op::Add || binop->op == Op::Sub);
        }

        std::unique_ptr<Instruction> emit(const TreeNode& node) const override {
            auto* store = std::get_if<StoreNode>(&node.data);
            auto* binop = std::get_if<BinOpNode>(&store->addr->data);

            Variable base;
            Number   offset;

            if (std::holds_alternative<Variable>(binop->left->data) &&
                std::holds_alternative<Number>(binop->right->data)) {
                base   = std::get<Variable>(binop->left->data);
                offset = std::get<Number>(binop->right->data);
            } else if (std::holds_alternative<Number>(binop->left->data) &&
                       std::holds_alternative<Variable>(binop->right->data) &&
                       binop->op == Op::Add) {
                offset = std::get<Number>(binop->left->data);
                base   = std::get<Variable>(binop->right->data);
            } else {
                return nullptr;
            }

            std::string val_str = tree_to_string(*store->value);

            return std::make_unique<RawL2Instruction>(
                "\tmem " + base.to_string() + " " +
                std::to_string(offset.getValue()) + " <- " + val_str + "\n"
            );
        }
    };  

} // namespace L3