#pragma once

#include <memory>
#include <variant>
#include <vector>
#include <set>
#include <type_traits>
#include <stdexcept>
#include <cassert>

#include "ast_leaves.h"   

namespace L3 {

    // root : AssignNode 

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

    struct BrNode {
        std::unique_ptr<TreeNode> cond;
        Label true_label;
    };


    struct LoadNode {
        std::unique_ptr<TreeNode> addr;
    };

    struct ReturnNode {
        std::unique_ptr<TreeNode> value;  // nullptr for void return
    };

    struct StoreNode {
        std::unique_ptr<TreeNode> addr;
    };

    struct CallNode {
        Callee callee;
        std::vector<std::unique_ptr<TreeNode>> args;
    };

    struct AssignNode {
        std::unique_ptr<TreeNode> dest;
        std::unique_ptr<TreeNode> src;
    };

    struct TreeNode {
        std::variant<Variable, Number, FunctionName,
                     BinOpNode, CompareNode,
                     LoadNode, StoreNode, AssignNode, CallNode, ReturnNode, BrNode> data;

        template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, TreeNode>>>
        TreeNode(T&& v) : data(std::forward<T>(v)) {}

        // this is only for S, T, U 
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

        if (auto* assign = std::get_if<AssignNode>(&node->data)) {

            if (std::holds_alternative<StoreNode>(assign->dest->data)) {
                if (replace_leaf(assign->dest, target, replacement)) return true;
            }
            return replace_leaf(assign->src, target, replacement);
        }

        if (auto* ret = std::get_if<ReturnNode>(&node->data)) {
            if (ret->value) return replace_leaf(ret->value, target, replacement);
            return false;
        }

        if (auto* var = std::get_if<Variable>(&node->data)) {
                if (var->name == target.name) {
                    node = std::move(replacement);   // splice in the replacement subtree
                    return true;
                }
                return false;   // wrong variable
        }

        if (auto* br = std::get_if<BrNode>(&node->data)) {
            if (br->cond) return replace_leaf(br->cond, target, replacement);
            return false;
        }

        if (auto* callnode = std::get_if<CallNode>(&node->data)){
            
            if (auto* callee_var = std::get_if<Variable>(&callnode->callee)) {
                if (callee_var->name == target.name) {
                 
                    return false;   // signal "found but not inlinable"
                }
            }
            for (auto& arg : callnode->args){
            if (replace_leaf(arg, target, replacement)) return true;
            }
            return false;
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
            return false;
        }

        // Number leaf, or any node type without children — nothing to replace.
        return false;
    }


    // Example of merging:
    // tree[i]: AssignNode { dest: %v, src: (3 op 1) }
    // tree[j]: AssignNode { dest: %ar, src: call allocate(%v, 1) }

    // After merge:
    // tree[j]: AssignNode { dest: %ar, src: call allocate((3 op 1), 1) }


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


    inline std::unique_ptr<TreeNode> clone_tree(const TreeNode& node) {
        return std::visit([](const auto& data) -> std::unique_ptr<TreeNode> {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, Variable> || std::is_same_v<T, Number> ||std::is_same_v<T, FunctionName>) {
                // leaf nodes — just copy the value
                return std::make_unique<TreeNode>(data);

            } else if constexpr (std::is_same_v<T, BinOpNode>) {
                return std::make_unique<TreeNode>(BinOpNode{
                    data.op,
                    clone_tree(*data.left),
                    clone_tree(*data.right)
                });
            } else if constexpr (std::is_same_v<T, BrNode>) {
                return std::make_unique<TreeNode>(BrNode{
                    data.cond ? clone_tree(*data.cond) : nullptr,
                    data.true_label
                });
            } else if constexpr (std::is_same_v<T, ReturnNode>) {
                return std::make_unique<TreeNode>(ReturnNode{
                    data.value ? clone_tree(*data.value) : nullptr
                });
            } else if constexpr (std::is_same_v<T, CallNode>) {
                    std::vector<std::unique_ptr<TreeNode>> cloned_args;
                    for (const auto& arg : data.args) {
                        cloned_args.push_back(clone_tree(*arg));
                    }
                    return std::make_unique<TreeNode>(CallNode{
                        data.callee,
                        std::move(cloned_args)
                    });

            } else if constexpr (std::is_same_v<T, CompareNode>) {
                return std::make_unique<TreeNode>(CompareNode{
                    data.op,
                    clone_tree(*data.left),
                    clone_tree(*data.right)
                });

            } else if constexpr (std::is_same_v<T, LoadNode>) {
                return std::make_unique<TreeNode>(LoadNode{
                    clone_tree(*data.addr)
                });

            } else if constexpr (std::is_same_v<T, StoreNode>) {
                return std::make_unique<TreeNode>(StoreNode{
                    clone_tree(*data.addr)
                });

            } else if constexpr (std::is_same_v<T, AssignNode>) {
                return std::make_unique<TreeNode>(AssignNode{
                    clone_tree(*data.dest),
                    clone_tree(*data.src)
                });
            }
        }, node.data);
    }


  

    inline std::string tree_to_string(const TreeNode& node) {
        return std::visit([](const auto& data) -> std::string {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, Variable> || std::is_same_v<T, Number> || std::is_same_v<T, FunctionName>) {
                return data.to_string();
            } else if constexpr (std::is_same_v<T, BrNode>) {
                std::string s = data.cond ? "br " + tree_to_string(*data.cond) : "br";
                s += " " + data.true_label.to_string();
                return s;
            } else if constexpr (std::is_same_v<T, ReturnNode>) {
                return data.value ? "return " + tree_to_string(*data.value) : "return";
            } else if constexpr (std::is_same_v<T, CallNode>) {
                std::string s = "call ";
                s += std::visit([](const auto& c) -> std::string {
                    using V = std::decay_t<decltype(c)>;
                    if constexpr (std::is_same_v<V, BuiltinCallee>) return builtinCalleeToString(c);
                    else if constexpr (std::is_same_v<V, FunctionName>) return "@" + c.name;
                    else return c.to_string();
                }, data.callee);
                s += "(";
                for (size_t i = 0; i < data.args.size(); i++) {
                    if (i) s += ", ";
                    s += tree_to_string(*data.args[i]);
                }
                s += ")";
                return s;

            } else if constexpr (std::is_same_v<T, BinOpNode>) {
                return "(" + tree_to_string(*data.left) 
                    + " op " 
                    + tree_to_string(*data.right) + ")";
            } else if constexpr (std::is_same_v<T, CompareNode>) {
                return "(" + tree_to_string(*data.left) 
                    + " cmp " 
                    + tree_to_string(*data.right) + ")";
            } else if constexpr (std::is_same_v<T, LoadNode>) {
                return "load(" + tree_to_string(*data.addr) + ")";
            } else if constexpr (std::is_same_v<T, StoreNode>) {
                return "store(" + tree_to_string(*data.addr) + ")";
            } else if constexpr (std::is_same_v<T, AssignNode>) {
                return tree_to_string(*data.dest) 
                    + " <- " 
                    + tree_to_string(*data.src);
            }
            return "unknown";
        }, node.data);
    }


    inline std::set<Variable> tree_reads(const TreeNode& node) {
        return std::visit([](const auto& data) -> std::set<Variable> {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, Variable>) {
                return {data};
            } else if constexpr (std::is_same_v<T, BrNode>) {
                return data.cond ? tree_reads(*data.cond) : std::set<Variable>{};
            } else if constexpr (std::is_same_v<T, Number>) {
                return {};

            } else if constexpr (std::is_same_v<T, FunctionName>) {
                return {};

            } else if constexpr (std::is_same_v<T, ReturnNode>) {
                return data.value ? tree_reads(*data.value) : std::set<Variable>{};
            } else if constexpr (std::is_same_v<T, CallNode>) {
                std::set<Variable> r;
                
                if (auto* var = std::get_if<Variable>(&data.callee)) {
                    r.insert(*var);
                }
                // all args are reads
                for (const auto& arg : data.args) {
                    auto rr = tree_reads(*arg);
                    r.insert(rr.begin(), rr.end());
                }
                return r;
            } else if constexpr (std::is_same_v<T, BinOpNode>) {
                auto r = tree_reads(*data.left);
                auto rr = tree_reads(*data.right);
                r.insert(rr.begin(), rr.end());
                return r;

            } else if constexpr (std::is_same_v<T, CompareNode>) {
                auto r = tree_reads(*data.left);
                auto rr = tree_reads(*data.right);
                r.insert(rr.begin(), rr.end());
                return r;

            } else if constexpr (std::is_same_v<T, LoadNode>) {
                return tree_reads(*data.addr);

            } else if constexpr (std::is_same_v<T, StoreNode>) {
                return tree_reads(*data.addr);

            } else if constexpr (std::is_same_v<T, AssignNode>) {
                std::set<Variable> r = tree_reads(*data.src);
                if (std::holds_alternative<StoreNode>(data.dest->data)) {
                    auto dest_reads = tree_reads(*data.dest);
                    r.insert(dest_reads.begin(), dest_reads.end());
                }
                return r;
            }
            return {};
        }, node.data);
    }

    inline std::set<Variable> tree_writes(const TreeNode& node) {
        return std::visit([](const auto& data) -> std::set<Variable> {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, AssignNode>) {
                // only the top level dest is a write
                if (auto* var = std::get_if<Variable>(&data.dest->data)) {
                    return {*var};
                }
            }
            return {};
        }, node.data);
    }
   

}  // namespace L3
