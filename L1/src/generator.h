#pragma once

#include <l1.h>

namespace L1{


    class Converter {
        public:
            // overloads for each type
            static std::string toString(Register r) {
                return "%" + registerToString(r);
            }

            static std::string toString(const memoryAccess& mem) {
                return std::to_string(mem.size) + "(%" + registerToString(mem.x_value) + ")";
            }

            static std::string toString(const Label& l) {
                return "_" + l;
            }

            static std::string toString(const Number& n) {
                return "$" + std::to_string(n.getValue());
            }

            // dispatch variant
            static std::string toString(const VALUE& v) {
                return std::visit([](const auto& val) -> std::string {
                    return Converter::toString(val);
                }, v);
            }

            // check if VALUE is a number
            // static bool isNumber(const VALUE& v) {
            //     return std::holds_alternative<Number>(v);
            // }

            // static bool isRegister(const VALUE& v) {
            //     return std::holds_alternative<Register>(v);
            // }
    };

    static std::string registerToByteString(const VALUE& v) {
          assert(std::holds_alternative<Register>(v) && "valueToByte: VALUE must be a Register");
          Register r = std::get<Register>(v);

          switch (r) {
              case Register::rax: return "%al";
              case Register::rbx: return "%bl";
              case Register::rcx: return "%cl";
              case Register::rdx: return "%dl";
              case Register::rdi: return "%dil";
              case Register::rsi: return "%sil";
              case Register::rbp: return "%bpl";
              case Register::r8:  return "%r8b";
              case Register::r9:  return "%r9b";
              case Register::r10: return "%r10b";
              case Register::r11: return "%r11b";
              case Register::r12: return "%r12b";
              case Register::r13: return "%r13b";
              case Register::r14: return "%r14b";
              case Register::r15: return "%r15b";
              case Register::rsp: return "%spl";
          }
          return "";
      }


    class CodeGenerator {
    public:
        // ── dispatcher (must come after all overloads it calls) ──────────────
        static std::string generate(const Instruction& instr) {
            switch (instr.type) {
                case InstructionType::AssignFromS:
                case InstructionType::AssignFromMemory:
                case InstructionType::AssignMemoryFromS:
                case InstructionType::compareAssign:
                    return generate(static_cast<const AssignInstruction&>(instr));
                case InstructionType::WaopT:
                case InstructionType::WIncDecMemory:
                    return generate(static_cast<const ArithInstruction&>(instr));
                case InstructionType::WsopSx:
                case InstructionType::WsopN:
                    return generate(static_cast<const ShiftInstruction&>(instr));
                case InstructionType::WIncDec:
                    return generate(static_cast<const IncDecInstruction&>(instr));
                case InstructionType::MemoryIncDecT:
                    return generate(static_cast<const MemIncDecInstruction&>(instr));
                case InstructionType::WAtWWE:
                    return generate(static_cast<const WWWEInstruction&>(instr));
                case InstructionType::CallPrint:
                case InstructionType::CallInput:
                case InstructionType::CallAllocate:
                case InstructionType::CallTupleError:
                case InstructionType::CallTensorError:
                case InstructionType::CallUN:
                    return generate(static_cast<const CallInstruction&>(instr));
                case InstructionType::Return:
                    return generate(static_cast<const ReturnInstruction&>(instr));
                case InstructionType::Label:
                    return generate(static_cast<const LabelInstruction&>(instr));
                case InstructionType::Goto:
                    return generate(static_cast<const GotoInstruction&>(instr));
                case InstructionType::CJump:
                    return generate(static_cast<const CjumpInstruction&>(instr));
                default:
                    return "";
            }
        }

        static std::string generate(const Number& instr) {
             return "$" + std::to_string(instr.getValue());
        }

        static std::string generate(const ArithInstruction& instr) {

                if (instr.getAop() == AopType::AddEq){
                    return "\taddq " + Converter::toString(instr.getSrc().value()) + ", " + Converter::toString(instr.getDst().value()) + '\n';
                }else if (instr.getAop() == AopType::SubEq){
                    return "\tsubq " + Converter::toString(instr.getSrc().value()) + ", " + Converter::toString(instr.getDst().value()) + '\n';
                }else if (instr.getAop() == AopType::MulEq){
                    return "\timulq " + Converter::toString(instr.getSrc().value()) + ", " + Converter::toString(instr.getDst().value()) + '\n';
                }else if (instr.getAop() == AopType::AndEq){
                    return  "\tandq " + Converter::toString(instr.getSrc().value()) + ", " + Converter::toString(instr.getDst().value()) + '\n';
                }

                return "";
        }

        static std::string generate(const ShiftInstruction& instr) {
            if (instr.sop == SopType::LShift){
                return "\tsalq " + Converter::toString(instr.src.value()) + ", " + Converter::toString(instr.dst.value()) + '\n';
            }else if (instr.sop == SopType::RShift){
                return "\tsarq " + Converter::toString(instr.src.value()) + ", " + Converter::toString(instr.dst.value()) + '\n';
            }
            std::cerr << "invalid shift operation\n";
            return "";
            }
        
        static std::string generate(const CjumpInstruction& instr) {
            std::string result;
            const auto& cmpVal = instr.cmp_val.value();
            const VALUE& left  = *cmpVal.left;   // dereference unique_ptr
            const VALUE& right = *cmpVal.right;  // dereference unique_ptr

            // both numbers — evaluate at compile time
            if (std::holds_alternative<Number>(left) && std::holds_alternative<Number>(right)) {
                int64_t l = std::get<Number>(left).getValue();
                int64_t r = std::get<Number>(right).getValue();
                bool taken = false;
                switch (stringToCmpType(cmpVal.cmp)) {
                    case CmpType::Eq:  taken = (l == r); break;
                    case CmpType::Neq: taken = (l != r); break;
                    case CmpType::Lt:  taken = (l <  r); break;
                    case CmpType::Lte: taken = (l <= r); break;
                    case CmpType::Gt:  taken = (l >  r); break;
                    case CmpType::Gte: taken = (l >= r); break;
                }
                return taken ? "\tjmp _" + instr.label + "\n" : ""; // we need _ cause we are not using convertor

            }
            // std::cerr << "generating cjump with left\n";
            // runtime comparison
            result += "\tcmpq " + Converter::toString(right) + ", " + Converter::toString(left) + "\n";
            switch (stringToCmpType(cmpVal.cmp)) {
                case CmpType::Eq:  result += "\tje ";  break;
                case CmpType::Neq: result += "\tjne "; break;
                case CmpType::Lt:  result += "\tjl ";  break;
                case CmpType::Lte: result += "\tjle "; break;
                case CmpType::Gt:  result += "\tjg ";  break;
                case CmpType::Gte: result += "\tjge "; break;
            }
            result += "_" + instr.label + "\n";
            return result;
        }

        static std::string generate(const IncDecInstruction& instr) {
          if (instr.isIncrement){
              return "\tinc " + Converter::toString(instr.dst.value()) + "\n";
          } else {
            return "\tdec " + Converter::toString(instr.dst.value()) + "\n";
            }

        }

        static std::string generate(const MemIncDecInstruction& instr) {
            if (instr.aop == AopType::AddEq){
                return "\taddq " + Converter::toString(instr.src.value()) + ", " + Converter::toString(instr.mem) + "\n";
            } else if (instr.aop == AopType::SubEq){
                return "\tsubq " + Converter::toString(instr.src.value()) + ", " + Converter::toString(instr.mem) + "\n";
            }
            return "";
        }
          // more red flag !!!!! 
        static std::string generate(const AssignInstruction& instr) {
                if (instr.isCmpAssign()){
                    const auto& cmpVal = instr.getCmpVal().value();
                    const auto& left = *cmpVal.left;
                    const auto& right = *cmpVal.right;

                    // check if any of them are just numbers if so you need a specific ordering 
                    const auto& recieveReg = instr.getTo().value();
                    
                    std::string cmpStr;

                    if (std::holds_alternative<Number>(left) && std::holds_alternative<Number>(right)){
                        int64_t l = std::get<Number>(left).getValue();
                        int64_t r = std::get<Number>(right).getValue();

                        std::string result;
                        switch (stringToCmpType(cmpVal.cmp)) {
                            case CmpType::Eq:  result = (l == r ? "1" : "0"); break;
                            case CmpType::Neq: result = (l != r ? "1" : "0"); break;
                            case CmpType::Lt:  result = (l <  r ? "1" : "0"); break;
                            case CmpType::Lte: result = (l <= r ? "1" : "0"); break;
                            case CmpType::Gt:  result = (l >  r ? "1" : "0"); break;
                            case CmpType::Gte: result = (l >= r ? "1" : "0"); break;
                        }
                                                return cmpStr; // validate the solution in compile time. 
                    }
                    else if (std::holds_alternative<Number>(left) && !std::holds_alternative<Number>(right)){
                        cmpStr += "\tcmpq " + Converter::toString(left) + ", " + Converter::toString(right) + "\n";
                    }else if (!std::holds_alternative<Number>(left) && std::holds_alternative<Number>(right)){
                        cmpStr += "\tcmpq " + Converter::toString(right) + ", " + Converter::toString(left) + "\n";
                    }else{
                        cmpStr += "\tcmpq " + Converter::toString(left) + ", " + Converter::toString(right) + "\n";
                    }
                    
                    
                    switch (stringToCmpType(cmpVal.cmp)) {
                        case CmpType::Eq: cmpStr += "\tsete " + registerToByteString(recieveReg) + "\n"; break;
                        case CmpType::Neq: cmpStr += "\tsetne " + registerToByteString(recieveReg) + "\n"; break;
                        case CmpType::Lt: cmpStr += "\tsetl " + registerToByteString(recieveReg) + "\n"; break;
                        case CmpType::Lte: cmpStr += "\tsetle " + registerToByteString(recieveReg) + "\n"; break;
                        case CmpType::Gt: cmpStr += "\tsetg " + registerToByteString(recieveReg) + "\n"; break;
                        case CmpType::Gte: cmpStr += "\tsetge " + registerToByteString(recieveReg) + "\n"; break;
                    }
                    cmpStr += "\tmovzbq " + registerToByteString(recieveReg) + ", " + Converter::toString(instr.getTo().value()) + '\n';
                    return cmpStr;
                  }
                
                if (std::holds_alternative<Label>(instr.getFrom().value())){
                    return "\tmovq $" + Converter::toString(instr.getFrom().value()) + ", " + Converter::toString(instr.getTo().value()) + "\n";
                }
              // W <- t cmp t
                return "\tmovq " + Converter::toString(instr.getFrom().value()) + ", " + Converter::toString(instr.getTo().value()) + "\n";
        }

        static std::string generate(const WWWEInstruction& instr) {
            return "lea (" + Converter::toString(instr.base.value()) + ", " + Converter::toString(instr.idx.value()) + ", " + std::to_string(instr.scale) + "), " + Converter::toString(instr.dst.value()) + "\n";
        }

        static std::string generate(const CallInstruction& instr) {
                if (instr.type == InstructionType::CallPrint){
                    return "\tcall print # runtime system call\n";
                }
                if (instr.type == InstructionType::CallInput){
                    return "\tcall input # runtime system call\n";
                }
                if (instr.type == InstructionType::CallAllocate){
                    return "\tcall allocate # runtime system call\n";
                }
                
                // Call tensor-error note implemented in runtime system yet. 

                if (instr.type == InstructionType::CallUN){
                    std::string callee_str = Converter::toString(instr.callee.value());
                        std::string result;
                        if (instr.arg.value() > 6){
                            result += "\tsubq $" + std::to_string(instr.arg.value()*8 + 8) + ", %rsp #allocate space for arguments and return address\n";
                        }else{
                            result += "\tsubq $8, %rsp #allocate space for return address only\n";

                        }
                        if (std::holds_alternative<Label>(instr.callee.value())){
                            result += "\tjmp " + callee_str + "# callee jump\n";  
                        } else if (std::holds_alternative<Register>(instr.callee.value())){
                            result += "\tjmp *" + callee_str + "# callee jump but register\n";
                        }
                        return result;
            }
                //    call input # runtime system call

                return "Error in CallInstruction generation: unrecognized call type\n";
        }

        static std::string generate(const ReturnInstruction& instr, int numLocals = 0, int numArgs = 0) {
            std::string result;
            if (numLocals > 0){
                result += "\taddq $" + std::to_string(numLocals * 8 + numArgs * 8) + ", %rsp #Deallocate locals\n";
            }
            result += "\tretq\n";
            return result;
        }

        static std::string generate(const LabelInstruction& instr) {
             return "_" + instr.label + ":\n";
        }

        static std::string generate(const GotoInstruction& instr) {
            return "\tjmp _" + instr.label + " #goto\n";
        }

        // ── top-level entry points ────────────────────────────────────────────
        static std::string generate(const Function& function) {
            std::string result;
            result += "_" + function.getLabel() + ":\n";

            
            if (function.getNumLocals() > 0){
                result += "\tsubq $" + std::to_string(function.getNumLocals() * 8) + ", %rsp #Allocate locals\n";
            }
            for (auto& instruction : function.instructions) {
                if (instruction->type == InstructionType::Return) {
                    if (function.getNumArgs() <= 6){
                        result += generate(
                        static_cast<const ReturnInstruction&>(*instruction),
                        function.getNumLocals(),
                        0
                    );
                    }else{
                        result += generate(
                        static_cast<const ReturnInstruction&>(*instruction),
                        function.getNumLocals(),
                        function.getNumArgs()
                    );
                    }
                } else {
                    result += generate(*instruction);  // generic for everything else
                }
            }
            return result;
        }

        static std::string generate(const Program& program) {
            std::string result;
            for (auto& function: program.functions){
                result += CodeGenerator::generate(function);
            }
            return result;
        }
        
  };


  void generate_code(const Program& p);  

};