
#pragma once

#include <memory>
#include <variant>
#include <vector>
#include <set>
#include <type_traits>
#include <stdexcept>



namespace L3 {


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

        // Raw : only used in code generation to represent L2 leaq instruction
        Raw
    };

    enum class Register {
        // Caller-saved first (colors 0–8)
        rdi, rsi, rdx, rcx, r8, r9, rax, r10, r11,
        // Callee-saved last (colors 9–14)
        rbx, rbp, r12, r13, r14, r15,
        rsp
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

        // for liveness analysis 
        struct LivenessInfo {
            std::set<Variable> in;
            std::set<Variable> out;
        };


    // ---------- Shared helpers ----------

   
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

    
    

    










    
};