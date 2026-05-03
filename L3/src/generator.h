#pragma once

#include <l3.h>

namespace L3 {


    class CodeGenerator {
        public:
            static inline std::string currentFnPrefix; /// function name used to distingush the existance of two similar function names in two functions 


            constexpr static const std::array<Register, 7> ARG_REGS = {
                        Register::rdi,
                        Register::rsi,
                        Register::rdx,
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

            static inline std::string OptoString(Op o) {
                switch (o) {
                    case Op::Add: return "+";
                    case Op::Sub: return "-";
                    case Op::Mul: return "*";
                    case Op::And: return "&";
                    case Op::Shl: return "<<";
                    case Op::Shr: return ">>";
                }
                throw std::runtime_error("OptoString: unknown Op");
            }

            static inline std::string CmptoString(Cmp c) {
                switch (c) {
                    case Cmp::Lt: return "<";
                    case Cmp::Le: return "<=";
                    case Cmp::Eq: return "=";
                    case Cmp::Ge: return ">=";
                    case Cmp::Gt: return ">";
                }
                throw std::runtime_error("CmptoString: unknown Cmp");
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
                        throw std::runtime_error("generate: unhandled instruction type "
                                                + std::to_string(static_cast<int>(instr.type)));
                }
            }

            static std::string generate(const BrInstruction& instr) {
                assert(instr.verify());
                return "\tgoto :_" + currentFnPrefix + '_' + instr.getTarget().value().name + '\n';
            }

            static std::string generate(const LabelInstruction& instr) {
                assert(instr.verify());
                return "\t:_" + currentFnPrefix + '_' + instr.getLabel().value().name + '\n';
            }

            static std::string generate(const BrTInstruction& instr) {
                assert(instr.verify());

                const std::string cond = std::visit(
                    [](const auto& x) { return x.to_string(); },
                    instr.getCond().value());
                const std::string target = ":_" + currentFnPrefix + '_' + instr.getTarget().value().name;

                // L3 'br t :L' = jump to :L if t != 0.
                // L2 cjump form: cjump t1 cmp t2 :true_label :false_label
                // We compare cond = 0; if equal (cond is false), fall through; else jump.
                return "\tcjump " + cond + " = 0 :__brt_skip\n"
                    + "\tgoto " + target + '\n'
                    + "\t:__brt_skip\n";
            }


            static std::string generate(const LoadInstruction& instr) {
                assert(instr.verify());

                const std::string dst = instr.getDst().value().to_string();
                const std::string src = instr.getSrc().value().to_string();

                return '\t' + dst + " <- mem " + src + " 0\n";
            }

            static std::string generate(const StoreInstruction& instr) {
                    assert(instr.verify());

                    const std::string dst = instr.getDst().value().to_string();
                    const std::string src = std::visit([](const auto& x) -> std::string {
                        using V = std::decay_t<decltype(x)>;
                        if constexpr (std::is_same_v<V, Label>)        return ":" + x.name;
                        else if constexpr (std::is_same_v<V, FunctionName>) return "@" + x.name;
                        else                                                 return x.to_string();
                    }, instr.getSrc().value());

                    return "\tmem " + dst + " 0 <- " + src + '\n';
                }

            static std::string generate(const CallInstruction& instr) {
                std::string result;

                const auto& callee = instr.getCallee();
                assert(callee.has_value());
                const auto& args = instr.getArgs();
                // place args
                for (size_t i = 0; i < args.size(); ++i) {
                    std::string arg_str = std::visit(
                        [](const auto& x) { return x.to_string(); }, args[i]);

                    if (i < ARG_REGS.size()) {
                        result += '\t' + registerToString(ARG_REGS[i])
                                + " <- " + arg_str + '\n';
                    } else {
                        int64_t offset = -8 * static_cast<int64_t>(i - ARG_REGS.size() + 2);
                        result += "\tmem rsp " + std::to_string(offset)
                                + " <- " + arg_str + '\n';
                    }
                }

                bool is_builtin = std::holds_alternative<BuiltinCallee>(*callee);

                static int64_t retCounter = 0;
                std::string ret_label;
                if (!is_builtin) {
                    ret_label = "_ret_" + std::to_string(retCounter++);
                    result += "\tmem rsp -8 <- :" + ret_label + "\n";
                }

                // dispatch on callee variant
                std::string callee_str = std::visit([](const auto& c) -> std::string {
                    using V = std::decay_t<decltype(c)>;
                    if constexpr (std::is_same_v<V, BuiltinCallee>) {
                        switch (c) {
                            case BuiltinCallee::Print:       return "print";
                            case BuiltinCallee::Allocate:    return "allocate";
                            case BuiltinCallee::Input:       return "input";
                            case BuiltinCallee::TupleError:  return "tuple-error";
                            case BuiltinCallee::TensorError: return "tensor-error";
                        }
                        return "";
                    } else if constexpr (std::is_same_v<V, FunctionName>) {
                        return "@" + c.name;
                    } else {
                        return c.to_string();
                    }
                }, *callee);

                result += "\tcall " + callee_str + ' ' + std::to_string(args.size()) + '\n';

                // emit the return label so the callee has somewhere to jump back to
                if (!is_builtin) {
                    result += "\t:" + ret_label + '\n';
                }

                return result;
            }


            static std::string generate(const CmpInstruction& instr) {
                assert(instr.verify());

                auto tStr = [](const T& v) {
                    return std::visit([](const auto& x) { return x.to_string(); }, v);
                };

                const std::string dst = instr.getDst().value().to_string();
                std::string lhs = tStr(instr.getLhs().value());
                std::string rhs = tStr(instr.getRhs().value());
                Cmp cmp = instr.getCmp().value();

                // L2 only supports <, <=, =. Normalize > and >= by swapping operands.
                if (cmp == Cmp::Gt) { std::swap(lhs, rhs); cmp = Cmp::Lt; }
                if (cmp == Cmp::Ge) { std::swap(lhs, rhs); cmp = Cmp::Le; }

                return '\t' + dst + " <- " + lhs + ' ' + CmptoString(cmp) + ' ' + rhs + '\n';
            }

            static std::string generate(const OpInstruction& instr) {
                assert(instr.verify());

                auto tStr = [](const T& v) {
                    return std::visit([](const auto& x) { return x.to_string(); }, v);
                };

                const std::string dst = instr.getDst().value().to_string();
                const std::string lhs = tStr(instr.getLhs().value());
                const std::string rhs = tStr(instr.getRhs().value());
                const std::string op  = OptoString(instr.getOp().value());

                if (dst == rhs || dst == lhs){
                    if (dst != rhs) return '\t' + dst + ' ' + op + "= " + rhs + '\n';
                    else return '\t' + dst + ' ' + op + "= " + lhs + '\n';
                }

                std::string result;
                result += '\t' + dst + " <- " + lhs + '\n';
                result += '\t' + dst + ' ' + op + "= " + rhs + '\n';
                return result;
            }

            static std::string generate(const ReturnInstruction&) {
                return "\treturn\n";
            }


            static std::string generate(const ReturnTInstruction& instr) {
                
                std::string result;

                const auto& value = instr.getValue();
                assert(value.has_value());
                std::string v = std::visit([](const auto& x) { return x.to_string(); }, *value);
                result += "\trax <- " + v + "\n";
                result += "\treturn\n";
                return result;

            }

            static std::string generate(const AssignInstruction& instr) {
                assert(instr.verify());

                const std::string dst = instr.getDst().value().to_string();
                const std::string src = std::visit([](const auto& x) -> std::string {
                    using V = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<V, Label>) {
                        return ":" + x.name;   // label as value: emit :name
                    } else if constexpr (std::is_same_v<V, FunctionName>) {
                        return "@" + x.name;   // function name as value: emit @name
                    } else {
                        return x.to_string();  // Variable or Number
                    }
                }, instr.getSrc().value());

                return '\t' + dst + " <- " + src + '\n';
            }




            static std::string generate(const VarCallInstruction& instr) {
                std::string result;
                            
                const auto& callee = instr.getCallee();
                assert(callee.has_value());
                const auto& args = instr.getArgs();

               
                

                // Place args (registers first, stack for overflow)
                for (size_t i = 0; i < args.size(); ++i) {
                    std::string arg_str = std::visit(
                        [](const auto& x) { return x.to_string(); }, args[i]);

                    if (i < ARG_REGS.size()) {
                        result += '\t' + registerToString(ARG_REGS[i])
                                + " <- " + arg_str + '\n';
                    } else {
                        int64_t offset = -8 * static_cast<int64_t>(i - ARG_REGS.size() + 2);
                        result += "\tmem rsp " + std::to_string(offset)
                                + " <- " + arg_str + '\n';
                    }
                }

                bool is_builtin = std::holds_alternative<BuiltinCallee>(*callee);

                // Generate a fresh return label for non-builtin calls
                static int64_t retCounter = 0;
                std::string ret_label;
                if (!is_builtin) {
                    ret_label = "_ret_" + std::to_string(retCounter++);
                    result += "\tmem rsp -8 <- :" + ret_label + "\n";
                }

                // Build callee string based on variant alternative
                std::string callee_str = std::visit([](const auto& c) -> std::string {
                    using V = std::decay_t<decltype(c)>;
                    if constexpr (std::is_same_v<V, BuiltinCallee>) {
                        switch (c) {
                            case BuiltinCallee::Print:       return "print";
                            case BuiltinCallee::Allocate:    return "allocate";
                            case BuiltinCallee::Input:       return "input";
                            case BuiltinCallee::TupleError:  return "tuple-error";
                            case BuiltinCallee::TensorError: return "tensor-error";
                        }
                        return "";
                    } else if constexpr (std::is_same_v<V, FunctionName>) {
                        return "@" + c.name;
                    } else {  // Variable: indirect call
                        return c.to_string();
                    }
                }, *callee);

                // Emit the call
                result += "\tcall " + callee_str + ' ' + std::to_string(args.size()) + '\n';

                // Return label — only for non-builtin calls (control flow returns here)
                if (!is_builtin) {
                    result += "\t:" + ret_label + '\n';
                }

                // Capture return value
                const std::string dst = instr.getDst().value().to_string();
                result += '\t' + dst + " <- rax\n";

                return result;
            }
    };


  void generate_code(const Program& p);  

};