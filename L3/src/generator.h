#pragma once

#include <l3.h>

namespace L3{


    class CodeGenerator {
        public:
            constexpr static const std::array<Register, 6> ARG_REGS = {
                        Register::rdi,
                        Register::rsi,
                        Register::r10,
                        Register::rcx,
                        Register::r8,
                        Register::r9,
                    };


            constexpr static inline std::string registerToString(Register r) {
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

            static std::string generate(const Instruction& instr) {
                switch (instr.type) {
                    case InstructionType::AssignFromS:
                        return generate(static_cast<const AssignInstruction&>(instr));
                    case InstructionType::AssignFromOp:
                        return generate(static_cast<const OpInstruction&>(instr));
                    case InstructionType::AssignFromCmp:
                        return generate(static_cast<const CmpInstruction&>(instr));
                    case InstructionType::AssignFromLoad:
                        return generate(static_cast<const LoadInstruction&>(instr));
                    case InstructionType::AssignFromCall:
                        return generate(static_cast<const VarCallInstruction&>(instr));
                    case InstructionType::Store:
                        return generate(static_cast<const StoreInstruction&>(instr));
                    case InstructionType::Call:
                        return generate(static_cast<const CallInstruction&>(instr));
                    case InstructionType::Return:
                        return generate(static_cast<const ReturnInstruction&>(instr));
                    case InstructionType::ReturnT:
                        return generate(static_cast<const ReturnTInstruction&>(instr));
                    case InstructionType::Br:
                        return generate(static_cast<const BrInstruction&>(instr));
                    case InstructionType::BrT:
                        return generate(static_cast<const BrTInstruction&>(instr));
                    case InstructionType::Label:
                        return generate(static_cast<const LabelInstruction&>(instr));
                    case InstructionType::Unknown:
                    default:
                        return "";
                }
            }

            static std::string generate(const ReturnTInstruction& instr) {
                
                std::string result;

                const auto& value = instr.getValue();
                assert(value.has_value());
                std::string v = std::visit([](const auto& x) { return x.to_string(); }, *value);
                result += "\trax <- " + v + "\n";
                result += "\treturn";
                return result;

            }




            static std::string generate(const VarCallInstruction& instr) {
                std::string result;

                // 1. extract callee — must be a function name for this overload
                const auto& callee = instr.getCallee();
                assert(callee.has_value());
                const auto* fname = std::get_if<FunctionName>(&*callee);
                assert(fname && "VarCallInstruction: expected FunctionName callee");

                const auto& args = instr.getArgs();

                // 2. push return address into mem rsp -8
                result += "\tmem rsp -8 <- :" + fname->name + "_ret\n";

            
                

                for (size_t i = 0; i < args.size(); ++i) {
                    std::string arg_str = std::visit(
                        [](const auto& x) { return x.to_string(); }, args[i]);

                    if (i < ARG_REGS.size()) {
                        // register arg
                        result += '\t' + registerToString(ARG_REGS[i])
                                + " <- " + arg_str + '\n';
                    } else {
                        // stack arg: arg 7 → rsp -16, arg 8 → rsp -24, ...
                        int64_t offset = -8 * static_cast<int64_t>(i - ARG_REGS.size() + 2);
                        result += "\tmem rsp " + std::to_string(offset)
                                + " <- " + arg_str + '\n';
                    }
                }

                // 4. call instruction with arity
                result += "\tcall @" + fname->name
                        + ' ' + std::to_string(args.size()) + '\n';

                // 5. return label
                result += "\t:" + fname->name + "_ret\n";

                return result;
            }
  };


  void generate_code(const Program& p);  

};