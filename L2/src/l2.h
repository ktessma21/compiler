
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <cassert>
#include <optional>

namespace L2 {
    class ASTNode {
        public:
            virtual ~ASTNode() = default;
            virtual std::string to_string() const = 0;
            virtual bool verify() const { return true; }
    };

    // label is just another string. 
    using Label = std::string;

    enum class InstructionType {

        // default value
        Unknown, 
        // assignment instructions
        compareAssign,      // W <- t cmp t
        AssignFromMemory,   // W <- mem X M
        AssignFromS,        // W <- S
        AssignMemoryFromS,  // mem X M <- S
        AssignFromStack,  // w <- stack-arg M

        // arithmetic/logic
        WaopT,              // W aop t
        WsopSx,             // W sop sx
        WsopN,              // W sop number

        // increment/decrement
        WIncDec,            // W++ / W--
        MemoryIncDecT,      // mem X M += t
        WIncDecMemory,      // W += mem X M 

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
            bool verify() const override { return true; }
           
     
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
            int64_t getValue() const { return value;}
            bool verify() const override { return true; }
            
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

    enum class AopType { AddEq, SubEq, MulEq, AndEq };
    enum class SopType { LShift, RShift };
    enum class CmpType { Eq, Neq, Lt, Lte, Gt, Gte };


      

    inline CmpType stringToCmpType(const std::string& cmp) {
        if (cmp == "=")  return CmpType::Eq;
        if (cmp == "!=") return CmpType::Neq;
        if (cmp == "<")  return CmpType::Lt;
        if (cmp == "<=") return CmpType::Lte;
        if (cmp == ">")  return CmpType::Gt;
        if (cmp == ">=") return CmpType::Gte;
        assert(false && "unknown cmp operator");
        return CmpType::Eq; // unreachable
    }


    struct memoryAccess {
        std::variant<Register, std::string> base = Register::rsp;  // the string is associated with the variable 
        int64_t size = 0;
    };


    

    using VALUE = std::variant<memoryAccess, Register, Label, Number>;
   
    
    struct compareStruct {
            std::unique_ptr<VALUE>left;
            std::string cmp;
            std::unique_ptr<VALUE> right;
        };
    
    

    inline bool isAssignType(InstructionType t) {
        return t == InstructionType::compareAssign    ||
            t == InstructionType::AssignFromMemory ||
            t == InstructionType::AssignFromS      ||
            t == InstructionType::AssignMemoryFromS ||
            t == InstructionType::AssignFromStack;
    }

    inline std::string memBaseToString(const memoryAccess& m) {
        return std::visit([](const auto& b) -> std::string {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, Register>) return registerToString(b);
            else return b;  // std::string (variable name)
        }, m.base);
    }

    class CjumpInstruction : public Instruction {
        public:
            std::optional<compareStruct> cmp_val;
            Label label;

            CjumpInstruction() : Instruction(InstructionType::CJump) {
                // std::cerr << "vinitalized cjump instruction\n";

            }  // add this

            void setCmpVal(compareStruct& cv) { cmp_val = std::move(cv);} 
            void setLabel(const Label& lbl) { label = lbl;}
            
            std::string to_string() const override{
                auto valueToString = [](const VALUE& v) -> std::string {
                    return std::visit([](const auto& val) -> std::string {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<T, Register>) {
                            return registerToString(val);
                        } else if constexpr (std::is_same_v<T, memoryAccess>){
                            return "mem " + memBaseToString(val) + " " + std::to_string(val.size);
                        } else if constexpr (std::is_same_v<T, Label>) {
                            return val;
                        } else if constexpr (std::is_same_v<T, Number>) {
                            return val.to_string();
                        } else {
                            assert(false && "unknown VALUE type");
                            return "";
                        }
                    }, v);
                };

                std::string result;
                result += "\t\tcjump ";
                result += valueToString(*cmp_val->left);
                result += " " + cmp_val->cmp + " ";
                result += valueToString(*cmp_val->left);
                result += " " + label + "\n";
                return result;
            }

            bool verify() const override { return true; }


    };

    class AssignInstruction : public Instruction {
        public:
            std::optional<VALUE> from;
            std::optional<VALUE> to;
            std::optional<compareStruct> cmp_val;  // separate, not inside VALUE


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
            void setCmpVal(compareStruct cv) { cmp_val  = std::move(cv); }

            // getters
            InstructionType getType() const { return type; }
            
            const std::optional<VALUE>& getFrom() const { return from; }
            const std::optional<VALUE>& getTo()   const { return to; }
            const std::optional<compareStruct>& getCmpVal() const { return cmp_val; }

            // safety checks
            bool hasFrom() const { return from.has_value(); }
            bool hasTo()   const { return to.has_value(); }
            bool isCmpAssign() const {return type == InstructionType::compareAssign;}
            bool isComplete() const { 
                return type != InstructionType::Unknown && 
                    (from.has_value() || cmp_val.has_value()) && 
                    to.has_value() ; 
            }

            bool verify() const override { return isComplete(); }

            std::string to_string() const override {
                assert(isComplete());
                
                auto valueToString = [](const VALUE& v) -> std::string {
                    return std::visit([](const auto& val) -> std::string {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<T, Register>) {
                            return registerToString(val);
                        } else if constexpr (std::is_same_v<T, memoryAccess>){
                            return "mem " + memBaseToString(val) + " " + std::to_string(val.size);
                        } else if constexpr (std::is_same_v<T, Label>) {
                            return val;
                        } else if constexpr (std::is_same_v<T, Number>) {
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

                    case InstructionType::compareAssign:
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
    // class WaopT
    
    class CallInstruction : public Instruction {
        public:
            
            std::optional<VALUE> callee;    // for CallUN: the u (Register or Label)
            std::optional<int64_t> arg;     // for CallUN: the number argument

            void setCallee(VALUE v)   { callee = std::move(v); }
            void setNum(int64_t n)    { arg = n; }
            
            CallInstruction(InstructionType t, int64_t argument = 0) : Instruction(t) {
                assert(t == InstructionType::CallPrint    ||
                    t == InstructionType::CallInput    ||
                    t == InstructionType::CallAllocate ||
                    t == InstructionType::CallTupleError ||
                    t == InstructionType::CallTensorError ||
                    t == InstructionType::CallUN);

                    if (t == InstructionType::CallPrint){
                        arg = 1;
                    } else if (t == InstructionType::CallInput){
                        arg = 0;
                    } else if (t == InstructionType::CallAllocate){
                        arg = 2;
                    } else if (t == InstructionType::CallTensorError){
                        arg = -1;
                    } else if (t == InstructionType::CallUN){
                        arg = argument;
                    }
            }

            std::string to_string() const override {
                switch (type) {
                    case InstructionType::CallPrint:       return "\t\tcall print 1\n";
                    case InstructionType::CallInput:       return "\t\tcall input 0\n";
                    case InstructionType::CallAllocate:    return "\t\tcall allocate 2\n";
                    case InstructionType::CallTupleError:  return "\t\tcall tuple-error 3\n";
                    case InstructionType::CallTensorError: return "\t\tcall tensor-error\n";
                    case InstructionType::CallUN:          return "\t\tcall u" + std::to_string(arg.value()) + "\n";
                    default:                               return "";
                }
            }
        };


    class ReturnInstruction : public Instruction {
        public:
            ReturnInstruction() : Instruction(InstructionType::Return) {}
            std::string to_string() const override {
                    return "\t\treturn\n";
            }
           
    };

    class LabelInstruction : public Instruction {
        public:
            Label label = "";

            LabelInstruction(Label _label) : Instruction(InstructionType::Label), label(_label) {}
            std::string to_string() const override {
                    return "\t\t:" + label + '\n';
            }
            
    }; 
    
    class GotoInstruction : public Instruction {
        public:
            Label label = "";

            GotoInstruction(Label _label) : Instruction(InstructionType::Goto), label(_label) {}
            std::string to_string() const override {
                    return "\t\tgoto :" + label + '\n';
            }
           
    };




      
        class ArithInstruction : public Instruction {
        public:
            std::optional<VALUE> dst;   // W
            AopType aop;
            std::optional<VALUE> src;   // t or mem X M

            ArithInstruction(InstructionType t) : Instruction(t) {
                // std::cerr << "intialized arith instruction with type " << std::endl;
            }
            void setDst(VALUE v) { dst = std::move(v); }
            void setSrc(VALUE v) { src = std::move(v); }
            void setAop(AopType a) { aop = a; }

            const std::optional<VALUE>& getDst() const { return dst; }
            const std::optional<VALUE>& getSrc() const { return src; }
            AopType getAop()                     const { return aop; }

            std::string to_string() const override {
                auto aopStr = [](AopType a) -> std::string {
                    switch(a) {
                        case AopType::AddEq: return "+=";
                        case AopType::SubEq: return "-=";
                        case AopType::MulEq: return "*=";
                        case AopType::AndEq: return "&=";
                    }
                    return "";
                };
                auto valueToString = [](const VALUE& v) -> std::string {
                    return std::visit([](const auto& val) -> std::string {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<T, Register>)    return registerToString(val);
                        else if constexpr (std::is_same_v<T, Number>) return std::to_string(val.getValue());
                        else if constexpr (std::is_same_v<T, memoryAccess>) 
                            return "mem " + memBaseToString(val) + " " + std::to_string(val.size);
                        else return "";
                    }, v);
                };
                return "\t\t" + valueToString(dst.value()) + " " + aopStr(aop) + " " + valueToString(src.value()) + "\n";
            }

           
        };

        class ShiftInstruction : public Instruction {
        public:
            std::optional<VALUE> dst;   // W
            SopType sop;
            std::optional<VALUE> src;   // sx or number

            ShiftInstruction(InstructionType t) : Instruction(t) {}
            void setDst(VALUE v)   { dst = std::move(v); }
            void setSrc(VALUE v)   { src = std::move(v); }
            void setSop(SopType s) { sop = s; }

            std::string to_string() const override {
                auto sopStr = [](SopType s) -> std::string {
                    switch(s) {
                        case SopType::LShift: return "<<=";
                        case SopType::RShift: return ">>=";
                    }
                    return "";
                };
                auto valueToString = [](const VALUE& v) -> std::string {
                    return std::visit([](const auto& val) -> std::string {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<T, Register>)    return registerToString(val);
                        else if constexpr (std::is_same_v<T, Number>) return std::to_string(val.getValue());
                        else return "";
                    }, v);
                };
                return "\t\t" + valueToString(dst.value()) + " " + sopStr(sop) + " " + valueToString(src.value()) + "\n";
                   }
        };

        class IncDecInstruction : public Instruction {
        public:
            std::optional<VALUE> dst;  // W
            bool isIncrement;          // true = ++, false = --

            IncDecInstruction() : Instruction(InstructionType::WIncDec) {}
            void setDst(VALUE v)      { dst = std::move(v); }
            void setIsInc(bool inc)   { isIncrement = inc; }

            std::string to_string() const override {
                std::string dstStr = std::visit([](const auto& val) -> std::string {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, Register>) return registerToString(val);
                    else if constexpr (std::is_same_v<T, Label>) return val;
                    else return "";
                }, dst.value());
                return "\t\t" + dstStr + (isIncrement ? "++" : "--") + "\n";
            }
        };

        class WWWEInstruction : public Instruction {
        public:
            std::optional<VALUE> dst;   // W
            std::optional<VALUE> base;  // W
            std::optional<VALUE> idx;   // W
            int64_t scale;              

            WWWEInstruction() : Instruction(InstructionType::WAtWWE) {}
            void setDst(VALUE v)  { dst  = std::move(v); }
            void setBase(VALUE v) { base = std::move(v); }
            void setIdx(VALUE v)  { idx  = std::move(v); }
            void setScale(int64_t s) { scale = s; }

            std::string to_string() const override {
                return "\t\t" + registerToString(std::get<Register>(dst.value()))  + " @ " +
                    registerToString(std::get<Register>(base.value())) + " "   +
                    registerToString(std::get<Register>(idx.value()))  + " "   +
                    std::to_string(scale) + "\n";
            }
           
        };

        class MemIncDecInstruction : public Instruction {
        public:
            memoryAccess mem;
            AopType aop;               // += or -=
            std::optional<VALUE> src;  // t

            MemIncDecInstruction() : Instruction(InstructionType::MemoryIncDecT) {}
            void setMem(memoryAccess m)  { mem = m; }
            void setAop(AopType a)       { aop = a; }
            void setSrc(VALUE v)         { src = std::move(v); }

            std::string to_string() const override {
                std::string aopStr = (aop == AopType::AddEq) ? "+=" : "-=";
                std::string src_str = std::visit([](const auto& val) -> std::string {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, Register>) return registerToString(val);
                    else if constexpr (std::is_same_v<T, Number>) return std::to_string(val.getValue());
                    else return "";
                }, src.value());
                return "\t\tmem " + memBaseToString(mem) + " " + 
                    std::to_string(mem.size) + " " + aopStr + " " + src_str + "\n";
            }

            

        };






        /// functions 




    class Function : public ASTNode {
        Label label = "";
        
        public:
            
            std::vector<std::unique_ptr<Instruction>> instructions;
            int num_args = 0;
            bool args_set = false;

            Function() = default;
            std::string to_string() const override {
                std::string result;
                result += '\t';
                result += label + "\n\t\t";
                result += std::to_string(num_args) + '\n';
                for (auto& instruction : instructions) {
                    result += instruction->to_string();  // -> and semicolon
                }
                result += "\t)\n";
                return result;  // semicolon
            }

            // getters
            std::string getLabel()   const { return label; }
            int getNumArgs()         const { return num_args; }

            // setters
            void setLabel(std::string l)  { label = l; }
            void setNumArgs(int n)        { args_set = true; num_args = n; }

            // Function(const Function&) = delete;          
            // Function& operator=(const Function&) = delete; 
            // Function(Function&&) = default;                
            // Function& operator=(Function&&) = default;     


            


        
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

            bool verify() const override { 
                for (auto& function: functions){
                    if (function.getLabel() == label) return true;
                }
                std::cerr << "no matching function with a program label\n";
                return false;
            }

            // Program(const Program&) = delete;            
            // Program& operator=(const Program&) = delete; 
            // Program(Program&&) = default;                
            // Program& operator=(Program&&) = default;     

          
    };

}

