
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
            virtual std::string to_string() const = 0;
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
     
    };

    
    // this could be a variant or I mean a UNION
    class Number : public ASTNode {
        int64_t value;
        public:
            Number() : value(0) {}
            Number(int64_t _value) : value(_value) { }
            std::string to_string() const override {
                return std::to_string(value);
            }
    };

    class Pointer : public ASTNode {
        uint64_t value;
    public:
        Pointer() : value(0) {}
        Pointer(uint64_t _value) : value(_value) {}
        std::string to_string() const override {
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

     inline std::string registerToString(Register r) {
        switch (r) {
            case Register::rcx: return "rcx";
            case Register::rdi: return "rdi";
            case Register::rsi: return "rsi";
            case Register::rdx: return "rdx";
            case Register::r8:  return "r8";
            case Register::r9:  return "r9";
            case Register::rax: return "rax";
            case Register::rbx: return "rbx";
            case Register::rbp: return "rbp";
            case Register::r10: return "r10";
            case Register::r11: return "r11";
            case Register::r12: return "r12";
            case Register::r13: return "r13";
            case Register::r14: return "r14";
            case Register::r15: return "r15";
            case Register::rsp: return "rsp";
        }
        return "";
    }

    struct memoryAccess {
        Register x_value = Register::rsp;  // some sentinel default
        int64_t size = 0;
    };

    

    using VALUE = std::variant<memoryAccess, Register, Label,Number, Pointer>;
   
    
    struct compareAssignValue {
            std::unique_ptr<VALUE>left;
            std::string cmp;
            std::unique_ptr<VALUE> right;
        };
    
    

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
            std::optional<compareAssignValue> cmp_val;  // separate, not inside VALUE


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
            void setCmpVal(compareAssignValue cv) { cmp_val  = std::move(cv); }

            // getters
            InstructionType getType() const { return type; }
            
            const std::optional<VALUE>& getFrom() const { return from; }
            const std::optional<VALUE>& getTo()   const { return to; }

            // safety checks
            bool hasFrom() const { return from.has_value(); }
            bool hasTo()   const { return to.has_value(); }
            bool isComplete() const { 
                return type != InstructionType::Unknown && 
                    (from.has_value() || cmp_val.has_value()) && 
                    to.has_value() ; 
            }

           std::string to_string() const override {
                assert(isComplete());
                
                auto valueToString = [](const VALUE& v) -> std::string {
                    return std::visit([](const auto& val) -> std::string {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<T, Register>) {
                            return registerToString(val);
                        } else if constexpr (std::is_same_v<T, memoryAccess>){
                            return "mem " + registerToString(val.x_value) + " " + std::to_string(val.size);
                        } else if constexpr (std::is_same_v<T, Label>) {
                            return val;
                        } else if constexpr (std::is_same_v<T, Number>) {
                            return val.to_string();
                        } else if constexpr (std::is_same_v<T, Pointer>) {
                            return val.to_string();
                        } else {
                            assert(false && "unknown VALUE type");
                            return "";
                        }
                    }, v);
                };

                std::string result;
                result += "\t\t";

                switch (type) {
                    case InstructionType::AssignFromS:
                    case InstructionType::AssignFromMemory:
                        // W <- S  or  W <- mem X M
                        result += valueToString(to.value());
                        result += " <- ";
                        result += valueToString(from.value());
                        break;

                    case InstructionType::AssignMemoryFromS:
                        // mem X M <- S
                        result += valueToString(to.value());
                        result += " <- ";
                        result += valueToString(from.value());
                        break;

                    case InstructionType::CompareAssign:
                        // W <- t cmp t  -- need extra fields for cmp and second t
                        result += valueToString(to.value());
                        result += " <- ";
                        result += valueToString(*cmp_val->left);
                        result += " " + cmp_val->cmp + " ";
                        result += valueToString(*cmp_val->right);
                        // TODO: add cmp and second opernd fields to AssignInstruction
                        break;

                    default:
                        assert(false && "unknown assign type");
                }

                return result + "\n";
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
            std::string to_string() const override {
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
            std::string to_string() const override {
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
            std::string to_string() const override {
                std::string result;
                result += '\t';
                result += '(' + label + "\n\t\t";
                result += std::to_string(num_args) + ' ' + std::to_string(num_locals) + '\n';
                for (auto& instruction : instructions) {
                    result += instruction->to_string();  // -> and semicolon
                }
                result += "\t)\n";
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
            std::string to_string() const override {
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

