
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <cassert>
#include <optional>

namespace L1 {
    class ASTNode {
        public:
            virtual ~ASTNode() = default;
            virtual std::string to_string() = 0;
    };

    // label is just another string. 
    using Label = std::string;

    enum class InstructionType {

        // default value
        Unknown, 
        // assignment instructions
        CompareAssign,      // W <- t cmp t
        AssignFromMemory,   // W <- mem X M
        AssignFromS,        // W <- S
        AssignMemoryFromS,  // mem X M <- S

        // arithmetic/logic
        WaopT,              // W aop t
        WsopSx,             // W sop sx
        WsopN,              // W sop number

        // increment/decrement
        WIncDec,            // W++ / W--
        MemoryIncDecT,      // mem X M += t

        // address computation
        WAtWWE,             // W @ W W E

        // call instructions
        CallPrint,          // call print 1
        CallInput,          // call input 0
        CallAllocate,       // call allocate 2
        CallTupleError,     // call tuple-error 3
        CallTensorError,    // call tensor-error F
        CallUN,             // call u number

        // control flow
        CJump,              // cjump t cmp t label
        Goto,               // goto label
        Label,              // :label
        Return              // return

        
    };

    class Instruction : public ASTNode {
        public:
            InstructionType type;
            Instruction() = delete;

            Instruction(InstructionType t) : type(t) {}
            virtual ~Instruction() = default;
            std::string to_string() override { return ""; }
    };

    
    // this could be a variant or I mean a UNION
    class Number : public ASTNode {
        int64_t value;
        public:
            Number() : value(0) {}
            Number(int64_t _value) : value(_value) { }
            std::string to_string() override {
                return std::to_string(value);
            }
    };

    class Pointer : public ASTNode {
        uint64_t value;
    public:
        Pointer() : value(0) {}
        Pointer(uint64_t _value) : value(_value) {}
        std::string to_string() override {
            return std::to_string(value);
        }
    };
    
    enum class Register {
        // sx
        rcx,

        // a registers (includes sx)
        rdi,
        rsi,
        rdx,
        r8,
        r9,

        // w registers (includes a)
        rax,
        rbx,
        rbp,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15,

        // special
        rsp
    };


    

    struct memoryAccess {
        Register x_value = Register::rsp;  // some sentinel default
        int64_t size = 0;
    };

    using VALUE = std::variant<memoryAccess, Register, Label, Number, Pointer>;

    inline bool isAssignType(InstructionType t) {
        return t == InstructionType::CompareAssign    ||
            t == InstructionType::AssignFromMemory ||
            t == InstructionType::AssignFromS      ||
            t == InstructionType::AssignMemoryFromS;
    }

    class AssignInstruction : public Instruction {
        public:
            std::optional<VALUE> from;
            std::optional<VALUE> to;

            AssignInstruction() : Instruction(InstructionType::Unknown) {}
            AssignInstruction(InstructionType t) : Instruction(t) {
                assert(isAssignType(t));
            }

            // setters
            void setType(InstructionType t) {
                assert(isAssignType(t));
                type = t;
            }
            void setFrom(VALUE v) { from = std::move(v); }
            void setTo(VALUE v)   { to   = std::move(v); }

            // getters
            InstructionType getType() const { return type; }
            
            const std::optional<VALUE>& getFrom() const { return from; }
            const std::optional<VALUE>& getTo()   const { return to; }

            // safety checks
            bool hasFrom() const { return from.has_value(); }
            bool hasTo()   const { return to.has_value(); }
            bool isComplete() const { 
                return type != InstructionType::Unknown && 
                    from.has_value() && 
                    to.has_value(); 
            }

            std::string to_string() override {
                assert(isComplete());
                // fill in later
                return "";
            }
        };

   class CallInstruction : public Instruction {
        public:
            CallInstruction(InstructionType t) : Instruction(t) {
                assert(t == InstructionType::CallPrint    ||
                    t == InstructionType::CallInput    ||
                    t == InstructionType::CallAllocate ||
                    t == InstructionType::CallTupleError ||
                    t == InstructionType::CallTensorError ||
                    t == InstructionType::CallUN);
            }
            std::string to_string() override {
                switch (type) {
                    case InstructionType::CallPrint:       return "call print 1\n";
                    case InstructionType::CallInput:       return "call input 0\n";
                    case InstructionType::CallAllocate:    return "call allocate 2\n";
                    case InstructionType::CallTupleError:  return "call tuple-error 3\n";
                    case InstructionType::CallTensorError: return "call tensor-error\n";
                    case InstructionType::CallUN:          return "call\n";
                    default:                               return "";
                }
            }
        };


    class ReturnInstruction : public Instruction {
        public:
            ReturnInstruction() : Instruction(InstructionType::Return) {}
            std::string to_string() override {
                    return "return";
            }
    };


    
    class Function : public ASTNode {
        Label label = "";
        
        public:
            
            std::vector<std::unique_ptr<Instruction>> instructions;
            int num_args = 0;
            int num_locals = 0;
            bool args_set = false;
            bool local_set = false;

            Function() = default;
            std::string to_string() override {
                std::string result;
                result += '(' + label + '\n';
                result += std::to_string(num_args) + ' ' + std::to_string(num_locals) + '\n';
                for (auto& instruction : instructions) {
                    result += instruction->to_string();  // -> and semicolon
                }
                result += ')';
                return result;  // semicolon
            }

            // getters
            std::string getLabel()   const { return label; }
            int getNumArgs()         const { return num_args; }
            int getNumLocals()       const { return num_locals; }

            // setters
            void setLabel(std::string l)  { label = l; }
            void setNumArgs(int n)        { args_set = true; num_args = n; }
            void setNumLocals(int n)      { local_set = true; num_locals = n; }


        
    };

    // Note label[0] is always the head. 

    class Program : public ASTNode {
        public:
            Label label;
            std::vector<Function> functions;

            Program() = default;
            std::string to_string() override {
                std::string result;
                result += '(';
                result += label + "\n";
                for (auto& function: functions){
                    result += function.to_string();
                }
                result += ')';
                return result;
            }
    };

}

