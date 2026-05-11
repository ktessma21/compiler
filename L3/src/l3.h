
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <cassert>
#include <optional>
#include <type_traits>
#include <tree.h>
#include <ast_leaves.h>
#include <set>



namespace L3 {

    // forward declaration. 
    class Function;
    class Context;
    struct LivenessInfo;
    std::vector<LivenessInfo> compute_liveness(const Function& f);
    std::vector<LivenessInfo> compute_liveness(const Context& ctx);

            // Instruction Herarchy 
    class Instruction : public ASTNode {
        public:
            InstructionType type;
            Instruction() = delete;
            Instruction(InstructionType t) : type(t) {}
            virtual ~Instruction() = default;

            bool verify() const override { return true; }
            virtual std::string to_string() const = 0;
            virtual std::unique_ptr<TreeNode> to_tree() const { return nullptr; }
            virtual std::set<Variable> reads() const {return {};}
            virtual std::set<Variable> writes() const {return {};}


    };
    
    




    /* ============================================================
 * AssignInstruction  —  var <- s
 * ============================================================ */
class AssignInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<S>        src;
public:
    AssignInstruction() : Instruction(InstructionType::AssignFromS) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setSrc(S s)        { src = std::move(s); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<S>&        getSrc() const { return src; }

    bool verify() const override { return dst.has_value() && src.has_value(); }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !src.has_value()) return nullptr;
        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst),  
                std::make_unique<TreeNode>(*src)
            });
    }

    std::string to_string() const override {
        assert(verify());
        std::string rhs = std::visit([](const auto& x) -> std::string {
            using V = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<V, std::string>) return x;
            else                                          return x.to_string();
        }, *src);
        return "\t" + dst->to_string() + " <- " + rhs + "\n";
    }


    std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (!src.has_value()) return r;
            if (auto* var = std::get_if<Variable>(&*src)){
                r.insert(*var);
            }
            return r;
    }

    std::set<Variable> writes() const override {
            std::set<Variable> r;
            if (!dst.has_value()) return r;
            r.insert(dst.value());
            return r;
    }


};


/* ============================================================
 * OpInstruction  —  var <- t op t
 * ============================================================ */
class OpInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<T>        lhs;
    std::optional<Op>       op;
    std::optional<T>        rhs;
public:
    OpInstruction() : Instruction(InstructionType::AssignFromOp) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setLhs(T t)        { lhs = std::move(t); }
    void setOp(Op o)        { op  = o; }
    void setRhs(T t)        { rhs = std::move(t); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<T>&        getLhs() const { return lhs; }
    const std::optional<Op>&       getOp()  const { return op;  }
    const std::optional<T>&        getRhs() const { return rhs; }

    bool verify() const override {
        return dst.has_value() && lhs.has_value() && op.has_value() && rhs.has_value();
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !op.has_value() || !lhs.has_value() || !rhs.has_value()) return nullptr;
        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst), 
                std::make_unique<TreeNode>(BinOpNode{
                        op.value(),
                        std::make_unique<TreeNode>(*lhs),  
                        std::make_unique<TreeNode>(*rhs)
                    })
            });
        }
            
    

    static std::string opToString(Op o) {
        switch (o) {
            case Op::Add: return "+";
            case Op::Sub: return "-";
            case Op::Mul: return "*";
            case Op::And: return "&";
            case Op::Shl: return "<<";
            case Op::Shr: return ">>";
        }
        throw std::runtime_error("OpInstruction: unknown Op");
    }

    std::string to_string() const override {
        assert(verify());
        auto tStr = [](const T& v) {
            return std::visit([](const auto& x) { return x.to_string(); }, v);
        };
        return "\t" + dst->to_string() + " <- " +
               tStr(*lhs) + " " + opToString(*op) + " " + tStr(*rhs) + "\n";
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (lhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*lhs)) {
                r.insert(*var);
            }
        }
        if (rhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*rhs)) {
                r.insert(*var);
            }
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }
};


/* ============================================================
 * CmpInstruction  —  var <- t cmp t
 * ============================================================ */
class CmpInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<T>        lhs;
    std::optional<Cmp>      cmp;
    std::optional<T>        rhs;
public:
    CmpInstruction() : Instruction(InstructionType::AssignFromCmp) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setLhs(T t)        { lhs = std::move(t); }
    void setCmp(Cmp c)      { cmp = c; }
    void setRhs(T t)        { rhs = std::move(t); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<T>&        getLhs() const { return lhs; }
    const std::optional<Cmp>&      getCmp() const { return cmp; }
    const std::optional<T>&        getRhs() const { return rhs; }

    bool verify() const override {
        return dst.has_value() && lhs.has_value() && cmp.has_value() && rhs.has_value();
    }

    static std::string cmpToString(Cmp c) {
        switch (c) {
            case Cmp::Lt: return "<";
            case Cmp::Le: return "<=";
            case Cmp::Eq: return "=";
            case Cmp::Ge: return ">=";
            case Cmp::Gt: return ">";
        }
        throw std::runtime_error("CmpInstruction: unknown Cmp");
    }

    std::string to_string() const override {
        assert(verify());
        auto tStr = [](const T& v) {
            return std::visit([](const auto& x) { return x.to_string(); }, v);
        };
        return "\t" + dst->to_string() + " <- " +
               tStr(*lhs) + " " + cmpToString(*cmp) + " " + tStr(*rhs) + "\n";
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !cmp.has_value() || !lhs.has_value() || !rhs.has_value()) return nullptr;
        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst), 
                std::make_unique<TreeNode>(CompareNode{
                        cmp.value(),
                        std::make_unique<TreeNode>(*lhs),  
                        std::make_unique<TreeNode>(*rhs)
                    })
            });
        }
    
    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (lhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*lhs)) {
                r.insert(*var);
            }
        }
        if (rhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*rhs)) {
                r.insert(*var);
            }
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }
                
};


/* ============================================================
 * LoadInstruction  —  var <- load var
 * ============================================================ */
class LoadInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<Variable> src;
public:
    LoadInstruction() : Instruction(InstructionType::AssignFromLoad) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setSrc(Variable v) { src = std::move(v); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<Variable>& getSrc() const { return src; }

    bool verify() const override { return dst.has_value() && src.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\t" + dst->to_string() + " <- load " + src->to_string() + "\n";
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !src.has_value()) return nullptr;

        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst),
                std::make_unique<TreeNode>(
                    LoadNode{
                        std::make_unique<TreeNode>(*src)
                    }
                )
            }
            );
        }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (src.has_value()) {
            r.insert(*src);
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }



};


/* ============================================================
 * StoreInstruction  —  store var <- s
 * ============================================================ */
class StoreInstruction : public Instruction {
    std::optional<Variable> addr;
    std::optional<S>        value;
public:
    StoreInstruction() : Instruction(InstructionType::Store) {}

    void setDst(Variable v) { addr = std::move(v); }
    void setSrc(S s)        { value = std::move(s); }

    const std::optional<Variable>& getDst() const { return addr; }
    const std::optional<S>&        getSrc() const { return value; }

    bool verify() const override { return addr.has_value() && value.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string rhs = std::visit([](const auto& x) -> std::string {
            using V = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<V, std::string>) return x;
            else                                          return x.to_string();
        }, *value);
        return "\tstore " + addr->to_string() + " <- " + rhs + "\n";
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!addr.has_value() || !value.has_value()) return nullptr;
        return std::make_unique<TreeNode>(
            StoreNode{
                std::make_unique<TreeNode>(*addr),   // addr
                std::make_unique<TreeNode>(*value)   // value
            });
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (addr.has_value()) {
            r.insert(*addr);  // address being stored into — that's a READ of the address variable
        }
        if (value.has_value()) {
            if (auto* var = std::get_if<Variable>(&*value)) {
                r.insert(*var);  // value being stored, if it's a variable
            }
        }
        return r;
    }
    // store writes to memory, not to a variable
};

/* ============================================================
 * Helpers shared by call instructions
 * ============================================================ */
inline std::string calleeToString(const Callee& c) {
    return std::visit([](const auto& x) -> std::string {
        using V = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<V, BuiltinCallee>) return builtinCalleeToString(x);
        else if constexpr (std::is_same_v<V, FunctionName>) return x.name;  // includes '@'
        else                                                return x.to_string();
    }, c);
}

inline std::string argsToString(const std::vector<T>& args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) out += ", ";
        out += std::visit([](const auto& x) { return x.to_string(); }, args[i]);
    }
    return out;
}


/* ============================================================
 * VarCallInstruction  —  var <- call callee ( args )
 * ============================================================ */
class VarCallInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<Callee>   callee;
    std::vector<T>          args;
public:
    VarCallInstruction() : Instruction(InstructionType::AssignFromCall) {}

    void setDst(Variable v)    { dst = std::move(v); }
    void setCallee(Callee c)   { callee = std::move(c); }
    void addArg(T t)           { args.push_back(std::move(t)); }

    const std::optional<Variable>& getDst()    const { return dst; }
    const std::optional<Callee>&   getCallee() const { return callee; }
    const std::vector<T>&          getArgs()   const { return args; }

    bool verify() const override { return dst.has_value() && callee.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\t" + dst->to_string() + " <- call " +
               calleeToString(*callee) + "(" + argsToString(args) + ")\n";
    }


    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (callee.has_value()) {
            if (auto* var = std::get_if<Variable>(&*callee)) {
                r.insert(*var);  // callee is a function pointer in a variable
            }
        }
        for (const auto& arg : args) {
            if (auto* var = std::get_if<Variable>(&arg)) {
                r.insert(*var);  // each Variable arg is a read
            }
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !callee.has_value()) return nullptr;
        
        std::vector<std::unique_ptr<TreeNode>> arg_trees;
        for (const auto& arg : args) {
            arg_trees.push_back(std::make_unique<TreeNode>(arg));
        }

        return std::make_unique<TreeNode>(AssignNode{
            std::make_unique<TreeNode>(*dst),
            std::make_unique<TreeNode>(CallNode{
                *callee,
                std::move(arg_trees)
            })
        });
    }

};


/* ============================================================
 * CallInstruction  —  call callee ( args )
 * ============================================================ */
class CallInstruction : public Instruction {
    std::optional<Callee> callee;
    std::vector<T>        args;
public:
    CallInstruction() : Instruction(InstructionType::Call) {}

    void setCallee(Callee c) { callee = std::move(c); }
    void addArg(T t)         { args.push_back(std::move(t)); }

    const std::optional<Callee>& getCallee() const { return callee; }
    const std::vector<T>&        getArgs()   const { return args; }

    bool verify() const override { return callee.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\tcall " + calleeToString(*callee) +
               "(" + argsToString(args) + ")\n";
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (callee.has_value()) {
            if (auto* var = std::get_if<Variable>(&*callee)) {
                r.insert(*var);  // callee is a function pointer in a variable
            }
        }
        for (const auto& arg : args) {
            if (auto* var = std::get_if<Variable>(&arg)) {
                r.insert(*var);  // each Variable arg is a read
            }
        }
        return r;
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!callee.has_value()) return nullptr;

        std::vector<std::unique_ptr<TreeNode>> arg_trees;
        for (const auto& arg : args) {
            arg_trees.push_back(std::make_unique<TreeNode>(arg));
        }

        return std::make_unique<TreeNode>(CallNode{
            *callee,
            std::move(arg_trees)
        });
    }

};


/* ============================================================
 * ReturnInstruction  —  return
 * ============================================================ */
class ReturnInstruction : public Instruction {
public:
    ReturnInstruction() : Instruction(InstructionType::Return) {}

    bool verify() const override { return true; }

    std::string to_string() const override {
        return "\treturn\n";
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        return std::make_unique<TreeNode>(ReturnNode{ nullptr });
    }
};


/* ============================================================
 * ReturnTInstruction  —  return t
 * ============================================================ */
class ReturnTInstruction : public Instruction {
    std::optional<T> value;
public:
    ReturnTInstruction() : Instruction(InstructionType::ReturnT) {}

    void setValue(T t) { value = std::move(t); }
    const std::optional<T>& getValue() const { return value; }

    bool verify() const override { return value.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string v = std::visit([](const auto& x) { return x.to_string(); }, *value);
        return "\treturn " + v + "\n";
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (value.has_value()) {
            if (auto* var = std::get_if<Variable>(&*value)) {
                r.insert(*var);
            }
        }
        return r;
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!value.has_value()) return nullptr;
        return std::make_unique<TreeNode>(ReturnNode{
            std::make_unique<TreeNode>(*value)
        });
    }
};


/* ============================================================
 * BrInstruction  —  br label
 * ============================================================ */
class BrInstruction : public Instruction {
    std::optional<Label> target;
public:
    BrInstruction() : Instruction(InstructionType::Br) {}

    void setTarget(Label l) { target = std::move(l); }
    const std::optional<Label>& getTarget() const { return target; }

    bool verify() const override { return target.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\tbr " + target->to_string() + "\n";
    }
};


/* ============================================================
 * BrTInstruction  —  br t label
 * ============================================================ */
class BrTInstruction : public Instruction {
    std::optional<T>     cond;
    std::optional<Label> target;
public:
    BrTInstruction() : Instruction(InstructionType::BrT) {}

    void setCond(T t)       { cond = std::move(t); }
    void setTarget(Label l) { target = std::move(l); }

    const std::optional<T>&     getCond()   const { return cond; }
    const std::optional<Label>& getTarget() const { return target; }

    bool verify() const override { return cond.has_value() && target.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string c = std::visit([](const auto& x) { return x.to_string(); }, *cond);
        return "\tbr " + c + " " + target->to_string() + "\n";
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (cond.has_value()) {
            if (auto* var = std::get_if<Variable>(&*cond)) {
                r.insert(*var);
            }
        }
        return r;
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!cond.has_value()) return nullptr;
        if (std::holds_alternative<Variable>(cond.value()))
            return std::make_unique<TreeNode>(*cond);
        else
            return std::make_unique<TreeNode>(*cond);
    }
};


/* ============================================================
 * LabelInstruction  —  :name (standalone)
 * ============================================================ */
class LabelInstruction : public Instruction {
    std::optional<Label> label;
public:
    LabelInstruction() : Instruction(InstructionType::Label) {}

    void setLabel(Label l) { label = std::move(l); }
    const std::optional<Label>& getLabel() const { return label; }

    bool verify() const override { return label.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\t" +  label->to_string() + "\n";   // no leading tab — labels are flush-left
    }
};

/* ============================================================
 * RawL2Instruction  —  has only string that is leaq. 
 * ============================================================ */

class RawL2Instruction : public Instruction {
    std::string text;
public:
    RawL2Instruction(std::string s) 
        : Instruction(InstructionType::Raw), text(std::move(s)) {}
    
    std::string to_string() const override { return text; }
};






    


    class Function : public ASTNode {
            FunctionName name;
            std::vector<L3::Variable> params;
            
        public:
            
            std::vector<std::unique_ptr<Instruction>> instructions;
            std::vector<L3::Context> contexts;
            bool _is_context = false;

            Function() = default;

            const std::string& getName() const { return this -> name.name; }
            const std::vector<Variable>& getParams() const { return params; }
            int getNumParams() const { return static_cast<int>(params.size()); }

            void setName(FunctionName n) { this -> name = std::move(n); }
            void addParam(Variable v) { params.push_back(std::move(v)); }
            void setParams(std::vector<Variable> ps) { params = std::move(ps); }

            bool verify() const override {
                if (this->name.name.empty()) return false;
                return _is_context ? !contexts.empty() : !instructions.empty();
            }

            std::string to_string() const override;  // ← declaration only
            void build_blocks();


        };


    class Program : public ASTNode {
        public:
        std::vector<Function> functions;

        Program() = default;

        std::string to_string() const override {
            std::string result;
            for (auto& function : functions) {
                result += function.to_string();
                result += "\n";
            }
            return result;
        }

        bool verify() const override {
            for (auto& function : functions) {
                if (function.getName() == "main") return true;
            }
            throw std::runtime_error("@main function doesn't exist");
            return false;
        }
    };


     inline std::unique_ptr<Instruction> emit_from_tree(const TreeNode& node) {
        return std::visit([](const auto& data) -> std::unique_ptr<Instruction> {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, AssignNode>) {
                auto* dest_var = std::get_if<Variable>(&data.dest->data);
                if (!dest_var) return nullptr;

                return std::visit([dest_var](const auto& src) -> std::unique_ptr<Instruction> {
                    using S = std::decay_t<decltype(src)>;

                    if constexpr (std::is_same_v<S, Number>) {
                        auto instr = std::make_unique<AssignInstruction>();
                        instr->setDst(*dest_var);
                        instr->setSrc(src);
                        return instr;
                    } else if constexpr (std::is_same_v<S, Variable>) {
                        auto instr = std::make_unique<AssignInstruction>();
                        instr->setDst(*dest_var);
                        instr->setSrc(src);
                        return instr;
                    } else if constexpr (std::is_same_v<S, BinOpNode>) {
                        auto instr = std::make_unique<OpInstruction>();
                        instr->setDst(*dest_var);
                        instr->setOp(src.op);
                        std::visit([&instr](const auto& l) {
                            using L = std::decay_t<decltype(l)>;
                            if constexpr (std::is_same_v<L, Variable> || std::is_same_v<L, Number>)
                                instr->setLhs(l);
                        }, src.left->data);
                        std::visit([&instr](const auto& r) {
                            using R = std::decay_t<decltype(r)>;
                            if constexpr (std::is_same_v<R, Variable> || std::is_same_v<R, Number>)
                                instr->setRhs(r);
                        }, src.right->data);
                        return instr;
                    } else if constexpr (std::is_same_v<S, CompareNode>) {
                        auto instr = std::make_unique<CmpInstruction>();
                        instr->setDst(*dest_var);
                        instr->setCmp(src.op);
                        std::visit([&instr](const auto& l) {
                            using L = std::decay_t<decltype(l)>;
                            if constexpr (std::is_same_v<L, Variable> || std::is_same_v<L, Number>)
                                instr->setLhs(l);
                        }, src.left->data);
                        std::visit([&instr](const auto& r) {
                            using R = std::decay_t<decltype(r)>;
                            if constexpr (std::is_same_v<R, Variable> || std::is_same_v<R, Number>)
                                instr->setRhs(r);
                        }, src.right->data);
                        return instr;
                    } else if constexpr (std::is_same_v<S, LoadNode>) {
                        auto instr = std::make_unique<LoadInstruction>();
                        instr->setDst(*dest_var);
                        if (auto* src_var = std::get_if<Variable>(&src.addr->data)) {
                            instr->setSrc(*src_var);
                        }
                        return instr;
                    } else if constexpr (std::is_same_v<S, CallNode>) {
                        auto instr = std::make_unique<VarCallInstruction>();
                        instr->setDst(*dest_var);
                        instr->setCallee(src.callee);
                        for (const auto& arg : src.args) {
                            std::visit([&instr](const auto& a) {
                                using A = std::decay_t<decltype(a)>;
                                if constexpr (std::is_same_v<A, Variable> || std::is_same_v<A, Number>)
                                    instr->addArg(a);
                            }, arg->data);
                        }
                        return instr;
                    } else {
                        return nullptr;  // ← catches all other src types
                    }
                }, data.src->data);

            } else if constexpr (std::is_same_v<T, StoreNode>) {
                auto instr = std::make_unique<StoreInstruction>();
                if (auto* addr_var = std::get_if<Variable>(&data.addr->data)) {
                    instr->setDst(*addr_var);
                }
                std::visit([&instr](const auto& v) {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<V, Variable> || std::is_same_v<V, Number>)
                        instr->setSrc(v);
                }, data.value->data);
                return instr;

            } else if constexpr (std::is_same_v<T, CallNode>) {
                auto instr = std::make_unique<CallInstruction>();
                instr->setCallee(data.callee);
                for (const auto& arg : data.args) {
                    std::visit([&instr](const auto& a) {
                        using A = std::decay_t<decltype(a)>;
                        if constexpr (std::is_same_v<A, Variable> || std::is_same_v<A, Number>)
                            instr->addArg(a);
                    }, arg->data);
                }
                return instr;

            } else if constexpr (std::is_same_v<T, ReturnNode>) {
                if (!data.value) {
                    return std::make_unique<ReturnInstruction>();
                }
                auto instr = std::make_unique<ReturnTInstruction>();
                std::visit([&instr](const auto& v) {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<V, Variable> || std::is_same_v<V, Number>)
                        instr->setValue(v);
                }, data.value->data);
                return instr;

            } else {
                return nullptr;  // ← Variable, Number, BinOpNode etc at top level
            }
        }, node.data);
    }

  
}

// include context AFTER all L3 classes are defined
#include "context.h"
#include "tiles.h"