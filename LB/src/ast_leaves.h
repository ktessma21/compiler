#pragma once

#include <memory>
#include <variant>
#include <vector>
#include <set>
#include <map>
#include <type_traits>
#include <stdexcept>
#include <string>


namespace LB {


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
        std::string to_string() const { return name; }
        bool operator==(const Variable& o) const { return name == o.name; }
        bool operator<(const Variable& o) const { return name < o.name; }
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
        std::string to_string() const { return name; }
        bool operator==(const FunctionName&) const = default;
        bool operator<(const FunctionName& o) const { return name < o.name; }
    };

   
    



    /* ---------- enum definitions ---------- */

    // op ::= + | - | * | & | << | >> | cmp
    // cmp ::= < | <= | = | >= | >
    // LB unifies them into the same enum (same as LA).
    enum class Op {
        Add,    // +
        Sub,    // -
        Mul,    // *
        And,    // &
        Shl,    // <<
        Shr,    // >>
        Lt,     // <
        Le,     // <=
        Eq,     // =
        Ge,     // >=
        Gt,     // >
    };

    // type ::= int64([])* | tuple | code ; T ::= type | void
    enum class VarType {
        Int64,
        Tuple,
        Code,
        Void,
    };

    enum class InstructionType {
        Unknown,

        Decl,               // type name

        AssignFromT,        // name <- t
        AssignFromOp,       // name <- t op t

        ArrayLoad,          // name <- name([t])+
        ArrayStore,         // name([t])+ <- t
        Length,             // name <- length name t?
        NewArray,           // name <- new Array(args)
        NewTuple,           // name <- new Tuple(t)

        // LB-specific control flow
        If,                 // if (cond) label label
        While,              // while (cond) label label
        Goto,               // goto label
        Continue,           // continue
        Break,              // break

        Return,             // return
        ReturnT,            // return t
        Label,              // :name

        Call,               // name(args)
        AssignFromCall,     // name <- name(args)

        // Scope brackets (one pair per { ... } block)
        ScopeOpen,          // {
        ScopeClose,         // }

        // Raw : only used in code generation to carry lower-level text
        Raw
    };

    enum class Register {
        rdi, rsi, rdx, rcx, r8, r9, rax, r10, r11,
        rbx, rbp, r12, r13, r14, r15,
        rsp
    };


    struct Type : public ASTNode {
        VarType base = VarType::Int64;
        int64_t dims = 0;
        Type() = default;
        explicit Type(VarType b, int64_t d = 0) : base(b), dims(d) {}
        std::string to_string() const {
            switch (base) {
                case VarType::Int64: {
                    std::string s = "int64";
                    for (int64_t i = 0; i < dims; ++i) s += "[]";
                    return s;
                }
                case VarType::Tuple: return "tuple";
                case VarType::Code:  return "code";
                case VarType::Void:  return "void";
            }
            throw std::runtime_error("Type::to_string: unknown VarType");
        }
        bool operator==(const Type& o) const { return base == o.base && dims == o.dims; }
        bool operator<(const Type& o) const {
            if (base != o.base) return base < o.base;
            return dims < o.dims;
        }
    };


     /* ============================================================
     * Instruction base
     * ============================================================ */
    class Instruction : public ASTNode {
        public:
            InstructionType type;
            int64_t lineNumber = 0;

            Instruction() = delete;
            Instruction(InstructionType t, int64_t line = 0) : type(t), lineNumber(line) {}
            virtual ~Instruction() = default;

            void setLineNumber(int64_t line) { lineNumber = line; }
            int64_t getLineNumber() const { return lineNumber; }
            bool verify() const override { return true; }
            virtual std::string to_string() const = 0;
    };

    // forward declaration
    struct Scope;

    using ScopeItem = std::variant<
        std::unique_ptr<Instruction>,
        std::unique_ptr<Scope>
    >;

    struct Scope : public ASTNode {
        Scope* parent = nullptr;
        std::map<std::string, Type> declaredTypes;
        std::vector<ScopeItem> items;  
        std::map<std::string, std::string> nameMap;  


        std::string to_string() const override {
            std::string out = "\t{\n";
            for (const auto& item : items) {
                std::visit([&out](const auto& p) { out += p->to_string(); }, item);
            }
            out += "\t}\n";
            return out;
        }

        void add(std::unique_ptr<Instruction> instr) {
            items.push_back(std::move(instr));
        }

        void add(std::unique_ptr<Scope> child) {
            items.push_back(std::move(child));
        }
    };


    /* ---------- variants ---------- */
    // t ::= name | N
    using T = std::variant<Variable, Number>;

    // Callee ::= FunctionName | Variable
    using Callee = std::variant<FunctionName, Variable>;


    /* ---------- helpers ---------- */

    inline std::string opToString(Op o) {
        switch (o) {
            case Op::Add: return "+";
            case Op::Sub: return "-";
            case Op::Mul: return "*";
            case Op::And: return "&";
            case Op::Shl: return "<<";
            case Op::Shr: return ">>";
            case Op::Lt:  return "<";
            case Op::Le:  return "<=";
            case Op::Eq:  return "=";
            case Op::Ge:  return ">=";
            case Op::Gt:  return ">";
        }
        throw std::runtime_error("opToString: unknown Op");
    }


    extern int freshCounter;
    std::string freshName(const std::string& original);

}