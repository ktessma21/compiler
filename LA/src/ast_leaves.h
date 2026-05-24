#pragma once

#include <memory>
#include <variant>
#include <vector>
#include <set>
#include <type_traits>
#include <stdexcept>
#include <string>



namespace LA {


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





     // enum definitions 

    // op ::= + | - | * | & | << | >> | < | <= | = | >= | >
    //   LA merges arithmetic and comparison operators into one op.
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
        Int64,          // int64 (array dimensionality tracked separately)
        Tuple,          // tuple
        Code,           // code
        Void,           // void
    };

    enum class InstructionType {
        Unknown,

        // declaration
        Decl,               // type name

        // assignments
        AssignFromT,        // name <- t
        AssignFromOp,       // name <- t op t

        // arrays / tuples
        ArrayLoad,          // name <- name([t])+
        ArrayStore,         // name([t])+ <- t
        Length,             // name <- length name t?
        NewArray,           // name <- new Array( args )
        NewTuple,           // name <- new Tuple( t )

        // control flow
        Br,                 // br label
        BrT,                // br t label
        Return,             // return
        ReturnT,            // return t
        Label,              // :name (standalone)

        // calls
        Call,               // name ( args )
        AssignFromCall,     // name <- name ( args )

        // Raw : only used in code generation to represent lower-level instructions
        Raw
    };

    enum class Register {
        // Caller-saved first (colors 0–8)
        rdi, rsi, rdx, rcx, r8, r9, rax, r10, r11,
        // Callee-saved last (colors 9–14)
        rbx, rbp, r12, r13, r14, r15,
        rsp
    };


    // Type ::= int64([])* | tuple | code | void
    //   For Int64, `dims` is the number of [] array dimensions (0 == scalar int64).
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


      /* Variants  */
        // t ::= name | N
        using T = std::variant<Variable, Number>;



    // ---------- Shared helpers ----------

   
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

    
    


    
};