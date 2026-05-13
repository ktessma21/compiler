#pragma once

#include <memory>
#include <variant>
#include <vector>
#include <set>
#include <type_traits>
#include <stdexcept>



namespace IR {


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
        bool operator==(const Label& o) const { return name == o.name; }
        bool operator<(const Label& o) const { return name < o.name; }
        bool empty() const { return name.empty(); }
    };

    struct Variable : public ASTNode {
        std::string name;
        Variable() = default;
        explicit Variable(std::string s) : name(std::move(s)) {}
        std::string to_string() const { return "%" + name; }
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
        std::string to_string() const { return "@" + name; }
        bool operator==(const FunctionName&) const = default;
        bool operator<(const FunctionName& o) const { return name < o.name; }

    };





     // enum definitions

    // op ::= + | - | * | & | << | >> | < | <= | = | >= | >
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

    enum class BuiltinCallee {
        Print,          // print
        Input,          // input
        TupleError,     // tuple-error
        TensorError,    // tensor-error
    };

    // T ::= type | void
    // type ::= int64([])* | tuple | code
    enum class TypeKind {
        Void,
        Int64,      // int64, int64[], int64[][], ... (dims tracked separately)
        Tuple,
        Code,
    };

    struct Type : public ASTNode {
        TypeKind kind = TypeKind::Void;
        int dims = 0; // only meaningful for Int64
        std::vector<std::string> dim_sizes; // only meaningful for Int64
        Type() = default;
        Type(TypeKind k, int d = 0) : kind(k), dims(d), 
            dim_sizes(k == TypeKind::Int64 ? d : 0) // pre-size to match dims. 
         {}
        std::string to_string() const {
            switch (kind) {
                case TypeKind::Void:  return "void";
                case TypeKind::Tuple: return "tuple";
                case TypeKind::Code:  return "code";
                case TypeKind::Int64: {
                    std::string s = "int64";
                    for (int i = 0; i < dims; ++i) s += "[]";
                    return s;
                }
            }
            throw std::runtime_error("Type::to_string: unknown TypeKind");
        }
        bool operator==(const Type& o) const { return kind == o.kind && dims == o.dims; }
    };

    enum class InstructionType {
        Unknown,

        // declaration
        TypeDecl,           // type var

        // assignments
        AssignFromS,        // var <- s
        AssignFromOp,       // var <- t op t   (covers arithmetic and comparisons)
        AssignFromLength,   // var <- length var t   |   var <- length var
        AssignFromCall,     // var <- call callee ( args? )
        AssignFromNewArray, // var <- new Array(args)
        AssignFromNewTuple, // var <- new Tuple(t)

        // array / tuple indexing
        AssignFromIndex,    // var <- var([t])+
        StoreIndex,         // var([t])+ <- s

        // control flow
        Br,                 // br label
        BrT,                // br t label label
        Return,             // return
        ReturnT,            // return t
        Label,              // :name (standalone)

        // calls
        Call,               // call callee ( args? )

        // Raw : only used in code generation
        Raw
    };


      /* Variants  */
        // t ::= var | N
        using T = std::variant<Variable, Number>;

        // u ::= var | l
        using U = std::variant<Variable, FunctionName>;

        // s ::= t | l   = var | N | @function
        using S = std::variant<Variable, Number, FunctionName>;

        // callee ::= u | builtin
        using Callee = std::variant<Variable, FunctionName, BuiltinCallee>;

        // for liveness analysis
        struct LivenessInfo {
            std::set<Variable> in;
            std::set<Variable> out;
        };


    // ---------- Shared helpers ----------


    inline std::string builtinCalleeToString(BuiltinCallee b) {
        switch (b) {
            case BuiltinCallee::Print:       return "print";
            case BuiltinCallee::Input:       return "input";
            case BuiltinCallee::TupleError:  return "tuple-error";
            case BuiltinCallee::TensorError: return "tensor-error";
        }
        throw std::runtime_error("builtinCalleeToString: unknown BuiltinCallee");
    }

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