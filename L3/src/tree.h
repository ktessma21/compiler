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
        std::variant<Variable, Number,
                     BinOpNode, CompareNode,
                     LoadNode, StoreNode, AssignNode, CallNode, ReturnNode> data;

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

        if (auto* callnode = std::get_if<CallNode>(&node->data)){
            for (auto& arg : callnode->args){
               if (replace_leaf(arg,  target, replacement)) return true;
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

            if constexpr (std::is_same_v<T, Variable> || std::is_same_v<T, Number>) {
                // leaf nodes — just copy the value
                return std::make_unique<TreeNode>(data);

            } else if constexpr (std::is_same_v<T, BinOpNode>) {
                return std::make_unique<TreeNode>(BinOpNode{
                    data.op,
                    clone_tree(*data.left),
                    clone_tree(*data.right)
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


    inline  std::unique_ptr<TreeNode> shrink_tree(const TreeNode& node){
        return std::visit([](const auto& data) -> std::unique_ptr<TreeNode> {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, Variable> || std::is_same_v<T, Number>) {
                // leaf nodes — just copy the value
                return std::make_unique<TreeNode>(data);

            } else if constexpr (std::is_same_v<T, ReturnNode>) {
                return std::make_unique<TreeNode>(ReturnNode{
                    data.value ? shrink_tree(*data.value) : nullptr
                });
            } else if constexpr (std::is_same_v<T, BinOpNode>) {
                // first recursively shrink children
                auto left  = shrink_tree(*data.left);
                auto right = shrink_tree(*data.right);

                // if both children folded to numbers, fold this node too
                if (std::holds_alternative<Number>(left->data) && 
                    std::holds_alternative<Number>(right->data)) {
                    
                    long long lv = std::get<Number>(left->data).getValue();
                    long long rv = std::get<Number>(right->data).getValue();
                    long long result;

                    switch (data.op) {
                        case Op::Add: result = lv + rv; break;
                        case Op::Sub: result = lv - rv; break;
                        case Op::Mul: result = lv * rv; break;
                        case Op::And: result = lv & rv; break;
                        case Op::Shl: result = lv << rv; break;
                        case Op::Shr: result = lv >> rv; break;
                        default: throw std::runtime_error("unknown op in shrink_tree");
                    }
                    return std::make_unique<TreeNode>(Number{result});
                }

                // otherwise return partially folded node
                return std::make_unique<TreeNode>(BinOpNode{
                    data.op,
                    std::move(left),
                    std::move(right)
                });

            } else if constexpr (std::is_same_v<T, CompareNode>) {
                auto left  = shrink_tree(*data.left);
                auto right = shrink_tree(*data.right);

                if (std::holds_alternative<Number>(left->data) &&
                    std::holds_alternative<Number>(right->data)) {
                    long long lv = std::get<Number>(left->data).getValue();
                    long long rv = std::get<Number>(right->data).getValue();
                    long long result;
                    switch (data.op) {
                        case Cmp::Lt: result = lv <  rv; break;
                        case Cmp::Le: result = lv <= rv; break;
                        case Cmp::Eq: result = lv == rv; break;
                        case Cmp::Ge: result = lv >= rv; break;
                        case Cmp::Gt: result = lv >  rv; break;
                    }
                    return std::make_unique<TreeNode>(Number{result});
                }

                return std::make_unique<TreeNode>(CompareNode{
                    data.op,
                    std::move(left),
                    std::move(right)
                });

            } else if constexpr (std::is_same_v<T, LoadNode>) {
                return std::make_unique<TreeNode>(LoadNode{
                    shrink_tree(*data.addr)
                });

            } else if constexpr (std::is_same_v<T, StoreNode>) {
                return std::make_unique<TreeNode>(StoreNode{
                    shrink_tree(*data.addr)
                });

            } else if constexpr (std::is_same_v<T, AssignNode>) {
                return std::make_unique<TreeNode>(AssignNode{
                    clone_tree(*data.dest),
                    shrink_tree(*data.src)
                });
            } else if constexpr (std::is_same_v<T, CallNode>) {
                    std::vector<std::unique_ptr<TreeNode>> shrinked_args;
                    for (const auto& arg : data.args) {
                        shrinked_args.push_back(shrink_tree(*arg));
                    }
                    return std::make_unique<TreeNode>(CallNode{
                        data.callee,
                        std::move(shrinked_args)
                    });
                }         
            }, node.data);}


    inline std::string tree_to_string(const TreeNode& node) {
        return std::visit([](const auto& data) -> std::string {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, Variable>) {
                return data.to_string();
            } else if constexpr (std::is_same_v<T, Number>) {
                return data.to_string();

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

            } else if constexpr (std::is_same_v<T, Number>) {
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
                // dest is a write not a read, only traverse src
                return tree_reads(*data.src);
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
