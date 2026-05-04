
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <cassert>
#include <optional>

namespace L3 {


    // enum definitions 

    enum class Op {
        Add,    // +
        Sub,    // -
        Mul,    // *
        And,    // &
        Shl,    // 
        Shr,    // >>
    };

    enum class Cmp {
        Lt,     // 
        Le,     // <=
        Eq,     // =
        Ge,     // >=
        Gt,     // >
    };

    enum class BuiltinCallee {
        Print,          // print
        Allocate,       // allocate
        Input,          // input
        TupleError,     // tuple-error
        TensorError,    // tensor-error
    };

    enum class InstructionType {
        Unknown,

        // assignments
        AssignFromS,        // var <- s
        AssignFromOp,       // var <- t op t
        AssignFromCmp,      // var <- t cmp t
        AssignFromLoad,     // var <- load var
        AssignFromCall,     // var <- call callee ( args )

        // memory
        Store,              // store var <- s

        // control flow
        Br,                 // br label
        BrT,                // br t label
        Return,             // return
        ReturnT,            // return t
        Label,              // :name (standalone)

        // calls
        Call,               // call callee ( args )
    };

    enum class Register {
        // Caller-saved first (colors 0–8)
        rdi, rsi, rdx, rcx, r8, r9, rax, r10, r11,
        // Callee-saved last (colors 9–14)
        rbx, rbp, r12, r13, r14, r15,
        rsp
    };

    // ---------- Shared string-conversion helpers ----------

    






    inline std::string builtinCalleeToString(BuiltinCallee b) {
        switch (b) {
            case BuiltinCallee::Print:       return "print";
            case BuiltinCallee::Allocate:    return "allocate";
            case BuiltinCallee::Input:       return "input";
            case BuiltinCallee::TupleError:  return "tuple-error";
            case BuiltinCallee::TensorError: return "tensor-error";
        }
        throw std::runtime_error("builtinCalleeToString: unknown BuiltinCallee");
    }
    /* big class groups */
    class ASTNode {
        public:
            virtual ~ASTNode() = default;
            virtual std::string to_string() const = 0;
            virtual bool verify() const { return true; }
        };


    struct Label : public ASTNode {
        std::string name;
        Label() = default;
        explicit Label(std::string s) : name(std::move(s)) {}
        std::string to_string() const { return ":" + name; }
        bool operator==(const Label&) const = default;
        bool operator<(const Label& o) const { return name < o.name; }
        bool empty() const { return name.empty(); }
    };

    struct Variable : public ASTNode {
        std::string name;
        Variable() = default;
        explicit Variable(std::string s) : name(std::move(s)) {}
        std::string to_string() const { return "%" + name; }
        bool operator==(const Variable&) const = default;
        bool operator<(const Variable& o) const { return name < o.name; }
        bool operator==(const std::string& o) const { return name == o; }
        bool operator<(const std::string& o) const { return name < o; }
    };

    class Number : public ASTNode {
        int64_t value;
    public:
        Number() : value(0) {}
        Number(int64_t v) : value(v) {}
        std::string to_string() const { return std::to_string(value); }
        int64_t getValue() const { return value; }
        bool operator==(const Number& o) const { return value == o.value; }
        bool operator<(const Number& o) const { return value < o.value; }
    };


    struct FunctionName : public ASTNode {
        std::string name;
        FunctionName() = default;
        explicit FunctionName(std::string s) : name(std::move(s)) {}
        std::string to_string() const { return "@" + name; }
        bool operator==(const FunctionName&) const = default;
        bool operator<(const FunctionName& o) const { return name < o.name; }

    };
    /* Variants  */
    // t ::= var | N
    using T = std::variant<Variable, Number>;

    // u ::= var | l
    //   l is a function name like "@foo" — stored as std::string per your convention
    using U = std::variant<Variable, FunctionName>;

    // s ::= t | label | l
    //   = var | N | :label | @function
    using S = std::variant<Variable, Number, Label, FunctionName>;

    // callee ::= u | builtin
    using Callee = std::variant<Variable, FunctionName, BuiltinCallee>;


    // Instruction Herarchy 
    class Instruction : public ASTNode {
        public:
            InstructionType type;
            Instruction() = delete;
            Instruction(InstructionType t) : type(t) {}
            virtual ~Instruction() = default;

            bool verify() const override { return true; }
            virtual std::string to_string() const = 0;

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

    std::string to_string() const override {
        assert(verify());
        std::string rhs = std::visit([](const auto& x) -> std::string {
            using V = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<V, std::string>) return x;
            else                                          return x.to_string();
        }, *src);
        return "\t" + dst->to_string() + " <- " + rhs + "\n";
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
};


/* ============================================================
 * StoreInstruction  —  store var <- s
 * ============================================================ */
class StoreInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<S>        src;
public:
    StoreInstruction() : Instruction(InstructionType::Store) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setSrc(S s)        { src = std::move(s); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<S>&        getSrc() const { return src; }

    bool verify() const override { return dst.has_value() && src.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string rhs = std::visit([](const auto& x) -> std::string {
            using V = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<V, std::string>) return x;
            else                                          return x.to_string();
        }, *src);
        return "\tstore " + dst->to_string() + " <- " + rhs + "\n";
    }
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







    class Context {
        std::vector<std::unique_ptr<Instruction>> instructions;
        public:
            Context() = default;
            // Add instructions one at a time.
            void add(std::unique_ptr<Instruction> instr) {
                instructions.push_back(std::move(instr));
            }

            // Read-only access.
            const std::vector<std::unique_ptr<Instruction>>& get() const {
                return instructions;
            }

            bool empty() const { return instructions.empty(); }
            size_t size() const { return instructions.size(); }

            bool is_terminated() const {
                if (instructions.empty()) return false;
                InstructionType t = instructions.back()->type;
                return t == InstructionType::Br
                    || t == InstructionType::BrT
                    || t == InstructionType::Return
                    || t == InstructionType::ReturnT
                    || t == InstructionType::Call;
            }
    };




    class Function : public ASTNode {
            FunctionName name;
            std::vector<L3::Variable> params;
        public:
            std::vector<std::unique_ptr<Instruction>> instructions;
            std::vector<L3::Context> contexts;

            Function() = default;

            std::string to_string() const override {
                std::string result;
                result += "define " + this->name.name + "(";
                for (size_t i = 0; i < params.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += "%" + params[i].name;
                }
                result += ") {\n";
                for (auto& instruction : instructions) {
                    result += instruction->to_string();
                }
                result += "}\n";
                return result;
            }

            const std::string& getName() const { return this -> name.name; }
            const std::vector<Variable>& getParams() const { return params; }
            int getNumParams() const { return static_cast<int>(params.size()); }

            void setName(FunctionName n) { this -> name = std::move(n); }
            void addParam(Variable v) { params.push_back(std::move(v)); }
            void setParams(std::vector<Variable> ps) { params = std::move(ps); }

            bool verify() const override {
                return !this -> name.name.empty() && !instructions.empty();
            }

             void build_blocks() {
                Context current;
                for (auto& instr : instructions) {
                    // A label starts a new block (close the current one if non-empty).
                    // a context always start with Label or Call
                    if ((instr->type == InstructionType::Label || instr->type == InstructionType::Call) && !current.empty()) {
                        contexts.push_back(std::move(current));
                        current = Context{};
                    }
                    
                    current.add(std::move(instr));
                    
                    // A terminator ends the current block.
                    if (current.is_terminated()) {
                        contexts.push_back(std::move(current));
                        current = Context{};
                    }
                }
                if (!current.empty()) {
                    contexts.push_back(std::move(current));
                }
                instructions.clear();
            }



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
}