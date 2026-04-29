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
        //     std::cerr << "cjump: left=" << Converter::toString(left) 
        //   << " cmp=" << cmpVal.cmp 
        //   << " right=" << Converter::toString(right) << "\n";

            if (!std::holds_alternative<Number>(left) && std::holds_alternative<Number>(right)) {
            // right is number — put it first (immediate must be first in AT&T)
                result += "\tcmpq " + Converter::toString(right) + ", " + Converter::toString(left) + "\n";
                
                switch (stringToCmpType(cmpVal.cmp)) {
                    case CmpType::Eq:  result += "\tje ";  break;
                    case CmpType::Neq: result += "\tjne "; break;
                    case CmpType::Lt:  result += "\tjl ";  break;  
                    case CmpType::Lte: result += "\tjle "; break;  
                    case CmpType::Gt:  result += "\tjg ";  break;  
                    case CmpType::Gte: result += "\tjge "; break;  
                }
            } else if (std::holds_alternative<Number>(left) && !std::holds_alternative<Number>(right)) {
                // left is number — already in correct position (first)
                result += "\tcmpq " + Converter::toString(left) + ", " + Converter::toString(right) + "\n";
                // no flip needed — number is already first
                switch (stringToCmpType(cmpVal.cmp)) {
                    case CmpType::Eq:  result += "\tje ";  break;
                    case CmpType::Neq: result += "\tjne "; break;
                    case CmpType::Lt:  result += "\tjg ";  break;  // flipped
                    case CmpType::Lte: result += "\tjge "; break;  // flipped
                    case CmpType::Gt:  result += "\tjl ";  break;  // flipped
                    case CmpType::Gte: result += "\tjle "; break;  // flipped
                }
            } else {
                // both registers — cmpq left, right computes right - left
                result += "\tcmpq " + Converter::toString(left) + ", " + Converter::toString(right) + "\n";
                // must flip because cmpq computes right - left not left - right
                switch (stringToCmpType(cmpVal.cmp)) {
                    case CmpType::Eq:  result += "\tje ";  break;  // same
                    case CmpType::Neq: result += "\tjne "; break;  // same
                    case CmpType::Lt:  result += "\tjg ";  break;  // flipped
                    case CmpType::Lte: result += "\tjge "; break;  // flipped
                    case CmpType::Gt:  result += "\tjl ";  break;  // flipped
                    case CmpType::Gte: result += "\tjle "; break;  // flipped
                }
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
                    const auto& recieveReg = instr.getTo().value();

                    std::string cmpStr;

                    // Compile-time case: both operands are numbers
                    if (std::holds_alternative<Number>(left) && std::holds_alternative<Number>(right)){
                        int64_t l = std::get<Number>(left).getValue();
                        int64_t r = std::get<Number>(right).getValue();

                        bool result = false;
                        switch (stringToCmpType(cmpVal.cmp)) {
                            case CmpType::Eq:  result = (l == r); break;
                            case CmpType::Neq: result = (l != r); break;
                            case CmpType::Lt:  result = (l <  r); break;
                            case CmpType::Lte: result = (l <= r); break;
                            case CmpType::Gt:  result = (l >  r); break;
                            case CmpType::Gte: result = (l >= r); break;
                        }
                        // Materialize the boolean directly into the destination register
                        cmpStr += "\tmovq $" + std::string(result ? "1" : "0")
                            + ", " + Converter::toString(recieveReg) + "\n";
                        return cmpStr;
                    }

                    // Case A: left is number, right is register/variable
                    // Emit cmpq left, right  -> right is second -> FLIP the set
                    else if (std::holds_alternative<Number>(left) && !std::holds_alternative<Number>(right)){
                        cmpStr += "\tcmpq " + Converter::toString(left) + ", " + Converter::toString(right) + "\n";
                        switch (stringToCmpType(cmpVal.cmp)) {
                            case CmpType::Eq:  cmpStr += "\tsete  " + registerToByteString(recieveReg) + "\n"; break;
                            case CmpType::Neq: cmpStr += "\tsetne " + registerToByteString(recieveReg) + "\n"; break;
                            case CmpType::Lt:  cmpStr += "\tsetg  " + registerToByteString(recieveReg) + "\n"; break; // flipped
                            case CmpType::Lte: cmpStr += "\tsetge " + registerToByteString(recieveReg) + "\n"; break; // flipped
                            case CmpType::Gt:  cmpStr += "\tsetl  " + registerToByteString(recieveReg) + "\n"; break; // flipped
                            case CmpType::Gte: cmpStr += "\tsetle " + registerToByteString(recieveReg) + "\n"; break; // flipped
                        }
                    }

                    // Case B: left is register/variable, right is number
                    // Emit cmpq right, left  -> left is second -> NATURAL set (no flip)
                    else if (!std::holds_alternative<Number>(left) && std::holds_alternative<Number>(right)){
                        cmpStr += "\tcmpq " + Converter::toString(right) + ", " + Converter::toString(left) + "\n";
                        switch (stringToCmpType(cmpVal.cmp)) {
                            case CmpType::Eq:  cmpStr += "\tsete  " + registerToByteString(recieveReg) + "\n"; break;
                            case CmpType::Neq: cmpStr += "\tsetne " + registerToByteString(recieveReg) + "\n"; break;
                            case CmpType::Lt:  cmpStr += "\tsetl  " + registerToByteString(recieveReg) + "\n"; break; // natural
                            case CmpType::Lte: cmpStr += "\tsetle " + registerToByteString(recieveReg) + "\n"; break; // natural
                            case CmpType::Gt:  cmpStr += "\tsetg  " + registerToByteString(recieveReg) + "\n"; break; // natural
                            case CmpType::Gte: cmpStr += "\tsetge " + registerToByteString(recieveReg) + "\n"; break; // natural
                        }
                    }

                    // Case C: both are registers/variables
                    // Emit cmpq left, right  -> right is second -> FLIP the set
                    else {
                        cmpStr += "\tcmpq " + Converter::toString(left) + ", " + Converter::toString(right) + "\n";
                        switch (stringToCmpType(cmpVal.cmp)) {
                            case CmpType::Eq:  cmpStr += "\tsete  " + registerToByteString(recieveReg) + "\n"; break;
                            case CmpType::Neq: cmpStr += "\tsetne " + registerToByteString(recieveReg) + "\n"; break;
                            case CmpType::Lt:  cmpStr += "\tsetg  " + registerToByteString(recieveReg) + "\n"; break; // flipped
                            case CmpType::Lte: cmpStr += "\tsetge " + registerToByteString(recieveReg) + "\n"; break; // flipped
                            case CmpType::Gt:  cmpStr += "\tsetl  " + registerToByteString(recieveReg) + "\n"; break; // flipped
                            case CmpType::Gte: cmpStr += "\tsetle " + registerToByteString(recieveReg) + "\n"; break; // flipped
                        }
                    }

                    cmpStr += "\tmovzbq " + registerToByteString(recieveReg) + ", "
                        + Converter::toString(recieveReg) + "\n";
                    return cmpStr;
                }
                
                if (std::holds_alternative<Label>(instr.getFrom().value())){
                    // std::cerr << Converter::toString(instr.getFrom().value()) << std::endl;
                    return "\tmovq $" + Converter::toString(instr.getFrom().value()) + ", " + Converter::toString(instr.getTo().value()) + "\n";
                }
              // W <- t cmp t
                return "\tmovq " + Converter::toString(instr.getFrom().value()) + ", " + Converter::toString(instr.getTo().value()) + "\n";
        }

        static std::string generate(const WWWEInstruction& instr) {
            return "lea (" + Converter::toString(instr.base.value()) + ", " + Converter::toString(instr.idx.value()) + ", " + std::to_string(instr.scale) + "), " + Converter::toString(instr.dst.value()) + "\n";
        }

        static std::string generate(const CallInstruction& instr) {
            if (instr.type == InstructionType::CallPrint) {
                return "\tcall print\n";
            }
            if (instr.type == InstructionType::CallInput) {
                return "\tcall input\n";
            }
            if (instr.type == InstructionType::CallAllocate) {
                return "\tcall allocate\n";
            }
            if (instr.type == InstructionType::CallTupleError) {
                return "\tcall tuple_error\n";
            }
            if (instr.type == InstructionType::CallTensorError) {
                // if (instr.arg.)
                assert(instr.arg.has_value());
                int64_t num_args = instr.arg.value();

                if (num_args == 1){
                    return "\tcall array_tensor_error_null # runtime system call\n";
                }
                if (num_args == 3){
                    return "\tcall array_error # runtime system call\n";
                }
                assert(num_args == 4);
                return "\tcall tensor_error\n";  // else it has to be 4
                
            }

            if (instr.type == InstructionType::CallUN) {
                assert(instr.callee.has_value());
                assert(instr.arg.has_value());

                std::string result;
                int64_t num_args = instr.arg.value();

                // how many args go on stack (beyond the 6 register args)
                int64_t stack_args = std::max((int64_t)0, num_args - 6);

                // total stack space = stack_args * 8 + 8 (return address)
                int64_t stack_space = (stack_args + 1) * 8;

                // get callee string
                std::string callee_str;
                if (std::holds_alternative<Label>(instr.callee.value())) {
                    // @foo -> _foo
                    const std::string& lbl = std::get<Label>(instr.callee.value());
                    callee_str = "_" + lbl;  // strip @ add _
                } else if (std::holds_alternative<Register>(instr.callee.value())) {
                    callee_str = Converter::toString(instr.callee.value());
                }

                // write return address to stack manually
                // the return label is generated by the caller — we don't have it here
                // so we use the rsp-relative write pattern
                result += "\tsubq $" + std::to_string(stack_space) + ", %rsp\n";

                // jump to callee
                if (std::holds_alternative<Label>(instr.callee.value())) {
                    result += "\tjmp " + callee_str + "\n";
                } else {
                    result += "\tjmp *" + callee_str + "\n";  // indirect jump for register
                }

                return result;
            }

            return "# ERROR: unrecognized call type\n";
        }

        static std::string generate(const ReturnInstruction& instr, int numLocals = 0, int numArgs = 0) {
            std::string result;

            int64_t dealloc = 0;
            dealloc += numLocals * 8;                              // locals space
            dealloc += (numArgs > 6 ? (numArgs - 6) * 8 : 0);    // extra stack args beyond 6
            // dealloc += 8;                                          // always +8 for return address

            if (dealloc > 0) result += "\taddq $" + std::to_string(dealloc) + ", %rsp\n";
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
                    result += generate(
                        static_cast<const ReturnInstruction&>(*instruction),
                        function.getNumLocals(),
                        function.getNumArgs()
                    );
                } else {
                    result += generate(*instruction);
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