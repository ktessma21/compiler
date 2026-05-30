#pragma once

#include "lb.h"
#include "ast_leaves.h"
#include <string>
#include <variant>
#include <type_traits>
#include <stdexcept>
#include <set>

namespace LB {

    class CodeGenerator {
    public:
        static inline Function* currentFunction = nullptr;
        static inline std::set<std::string> functionNames = {};

        // LB variables have no % prefix
        static std::string vStr(const Variable& v) {
            return v.name;
        }

        static std::string tStr(const T& v) {
            return std::visit([](const auto& x) -> std::string {
                return x.to_string();
            }, v);
        }

        static std::string lStr(const Label& l) {
            if (!l.name.empty() && l.name.front() == ':') return l.name;
            return ":" + l.name;
        }

        static std::string callStr(const Callee& c) {
            return std::visit([](const auto& x) -> std::string {
                return x.name;
            }, c);
        }

        // ----- per-kind helpers -----

        static std::string generate(const DeclInstruction& instr) {
            return "\t" + instr.getType()->to_string() + " " + vStr(*instr.getVar()) + "\n";
        }

        static std::string generate(const AssignInstruction& instr) {
            return "\t" + vStr(*instr.getDst()) + " <- " + tStr(*instr.getSrc()) + "\n";
        }

        static std::string generate(const OpInstruction& instr) {
            return "\t" + vStr(*instr.getDst()) + " <- " +
                   tStr(*instr.getLhs()) + " " + opToString(*instr.getOp()) + " " +
                   tStr(*instr.getRhs()) + "\n";
        }

        static std::string generate(const ArrayLoadInstruction& instr) {
            std::string subs;
            for (const auto ix : instr.getIndices())
                subs += "[" + tStr(ix) + "]";
            return "\t" + vStr(*instr.getDst()) + " <- " + vStr(*instr.getSrc()) + subs + "\n";
        }

        static std::string generate(const ArrayStoreInstruction& instr) {
            std::string subs;
            for (const auto ix : instr.getIndices())
                subs += "[" + tStr(ix) + "]";

            std::string rhs;
            if (instr.getSrcCallee().has_value()) {
                rhs = callStr(*instr.getSrcCallee());
            } else {
                rhs = tStr(*instr.getSrc());
            }
            return "\t" + vStr(*instr.getDst()) + subs + " <- " + rhs + "\n";
        }

        static std::string generate(const LengthInstruction& instr) {
            std::string out = "\t" + vStr(*instr.getDst()) + " <- length " + vStr(*instr.getArray());
            if (instr.getDim().has_value())
                out += " " + tStr(*instr.getDim());
            return out + "\n";
        }

        static std::string generate(const NewArrayInstruction& instr) {
            std::string out = "\t" + vStr(*instr.getDst()) + " <- new Array(";
            const auto& args = instr.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) out += ", ";
                out += tStr(args[i]);
            }
            return out + ")\n";
        }

        static std::string generate(const NewTupleInstruction& instr) {
            return "\t" + vStr(*instr.getDst()) + " <- new Tuple(" + tStr(*instr.getSize()) + ")\n";
        }

        static std::string generate(const VarCallInstruction& instr) {
            std::string out = "\t" + vStr(*instr.getDst()) + " <- " + callStr(*instr.getCallee()) + "(";
            const auto& args = instr.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) out += ", ";
                out += tStr(args[i]);
            }
            return out + ")\n";
        }

        static std::string generate(const CallInstruction& instr) {
            std::string out = "\t" + callStr(*instr.getCallee()) + "(";
            const auto& args = instr.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) out += ", ";
                out += tStr(args[i]);
            }
            return out + ")\n";
        }

        static std::string generate(const ReturnInstruction&) {
            return "\treturn\n";
        }

        static std::string generate(const ReturnTInstruction& instr) {
            return "\treturn " + tStr(*instr.getValue()) + "\n";
        }

        static std::string generate(const LabelInstruction& instr) {
            return "\t" + lStr(*instr.getLabel()) + "\n";
        }

        static std::string generate(const RawInstruction& instr) {
            return instr.getText() + "\n";
        }

        // ----- dispatch -----
        static std::string generate(const Instruction& instr) {
            switch (instr.type) {
                case InstructionType::Decl:
                    return generate(static_cast<const DeclInstruction&>(instr));
                case InstructionType::AssignFromT:
                    return generate(static_cast<const AssignInstruction&>(instr));
                case InstructionType::AssignFromOp:
                    return generate(static_cast<const OpInstruction&>(instr));
                case InstructionType::ArrayLoad:
                    return generate(static_cast<const ArrayLoadInstruction&>(instr));
                case InstructionType::ArrayStore:
                    return generate(static_cast<const ArrayStoreInstruction&>(instr));
                case InstructionType::Length:
                    return generate(static_cast<const LengthInstruction&>(instr));
                case InstructionType::NewArray:
                    return generate(static_cast<const NewArrayInstruction&>(instr));
                case InstructionType::NewTuple:
                    return generate(static_cast<const NewTupleInstruction&>(instr));
                case InstructionType::AssignFromCall:
                    return generate(static_cast<const VarCallInstruction&>(instr));
                case InstructionType::Call:
                    return generate(static_cast<const CallInstruction&>(instr));
                case InstructionType::Return:
                    return generate(static_cast<const ReturnInstruction&>(instr));
                case InstructionType::ReturnT:
                    return generate(static_cast<const ReturnTInstruction&>(instr));
                case InstructionType::Label:
                    return generate(static_cast<const LabelInstruction&>(instr));
                case InstructionType::Raw:
                    return generate(static_cast<const RawInstruction&>(instr));
                case InstructionType::Unknown:
                default:
                    throw std::runtime_error("generate: unhandled instruction type "
                                             + std::to_string(static_cast<int>(instr.type)));
            }
        }
    };
}