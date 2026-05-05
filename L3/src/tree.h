#pragma once

#include <memory>
#include <variant>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include <cassert>

#include "ast_leaves.h"   

namespace L3 {

    struct TreeNode;  // forward declaration

    struct BinOpNode {
        Op op;
        std::unique_ptr<TreeNode> left;
        std::unique_ptr<TreeNode> right;
    };

    struct CompareNode {
        Cmp op;
        std::unique_ptr<TreeNode> left;
        std::unique_ptr<TreeNode> right;
    };

    struct LoadNode {
        std::unique_ptr<TreeNode> addr;
    };

    struct StoreNode {
        std::unique_ptr<TreeNode> addr;
        std::unique_ptr<TreeNode> value;   // you'll want this for full stores
    };

    struct AssignNode {
        std::unique_ptr<TreeNode> dest;
        std::unique_ptr<TreeNode> src;
    };

    struct TreeNode {
        std::variant<Variable, Number,
                     BinOpNode, CompareNode,
                     LoadNode, StoreNode, AssignNode> data;

        template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, TreeNode>>>
        TreeNode(T&& v) : data(std::forward<T>(v)) {}

        template <typename... Alts>
        TreeNode(const std::variant<Alts...>& v)
            : data(std::visit([](const auto& alt) -> decltype(data) {
                       using T = std::decay_t<decltype(alt)>;
                       if constexpr (std::is_constructible_v<decltype(data), const T&>) {
                           return decltype(data){alt};
                       } else {
                           throw std::runtime_error(
                               "operand alternative not valid in expression tree");
                       }
                   }, v))
        {}
    };


    inline bool replace_leaf(
        std::unique_ptr<TreeNode>& node,
        const Variable& target,
        std::unique_ptr<TreeNode>& replacement)
    {
        if (!node) return false;

        if (auto* var = std::get_if<Variable>(&node->data)) {
                if (var->name == target.name) {
                    node = std::move(replacement);   // splice in the replacement subtree
                    return true;
                }
                return false;   // wrong variable
        }

        if (auto* binop = std::get_if<BinOpNode>(&node->data)) {
            if (replace_leaf(binop->left,  target, replacement)) return true;
            if (replace_leaf(binop->right, target, replacement)) return true;
            return false;
        }

        if (auto* cmp = std::get_if<CompareNode>(&node->data)) {
            if (replace_leaf(cmp->left,  target, replacement)) return true;
            if (replace_leaf(cmp->right, target, replacement)) return true;
            return false;
        }

        if (auto* load = std::get_if<LoadNode>(&node->data)) {
            return replace_leaf(load->addr, target, replacement);
        }

        if (auto* store = std::get_if<StoreNode>(&node->data)) {
            if (replace_leaf(store->addr,  target, replacement)) return true;
            if (replace_leaf(store->value, target, replacement)) return true;
            return false;
        }

        // Number leaf, or any node type without children — nothing to replace.
        return false;
    }

    
    inline std::unique_ptr<TreeNode> merge_tree(
        std::unique_ptr<TreeNode> t1,
        std::unique_ptr<TreeNode> t2)
    {
        // t1 must be Assign(%var, expr) for merging to make sense
        // on my implementation assignNode only exist at the top and never below.
        auto* assign = std::get_if<AssignNode>(&t1->data);
        if (!assign) return nullptr;

        auto* dest_var = std::get_if<Variable>(&assign->dest->data);
        if (!dest_var) return nullptr;

        // Walk t2 looking for a leaf Variable matching *dest_var.
         // When found, replace it with assign->src (the value t1 computes).
        if (replace_leaf(t2, *dest_var, assign->src)) {
            return t2;       // success — t1 is gone, t2 now contains the inlined value
        }
        return nullptr;


    }

}  // namespace L3
