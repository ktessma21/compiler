#pragma once

#include "la.h"
#include "ast_leaves.h"
#include <string>
#include <variant>
#include <type_traits>
#include <stdexcept>

namespace LA {

    // ============================================================
    // generate.h  —  LA code generation (LA -> IR).
    //
    // IR differs from LA's own to_string() in one key way: every VARIABLE
    // must be printed with a leading '%'. LA's to_string() prints bare
    // names (correct for the LA dump, wrong for IR), so the generator
    // emits IR syntax itself here, prefixing variables via vStr().
    //
    // RawInstruction already holds final IR text and is emitted VERBATIM.
    // Labels are printed with a single leading ':'.
    // ============================================================

    class CodeGenerator {
    public:
        static inline Function* currentFunction = nullptr;

        // ----- IR-prefixing string helpers -----

        // A variable in IR is "%name".
        static std::string vStr(const Variable& v) {
            return "%" + v.name;
        }

        // A T operand: "%name" for a Variable, the literal for a Number.
        static std::string tStr(const T& v) {
            return std::visit([](const auto& x) -> std::string {
                using V = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<V, Variable>) return "%" + x.name;
                else                                       return x.to_string();
            }, v);
        }

        // A label in IR is ":name". Emit exactly one leading ':' even if the
        // stored name already carries one (prevents "::name").
        static std::string lStr(const Label& l) {
            if (!l.name.empty() && l.name.front() == ':') return l.name;
            return ":" + l.name;
        }

        // ----- per-kind helpers (emit IR syntax) -----

        static std::string generate(const DeclInstruction& instr) {
            // <type> %name
            return "\t" + instr.getType()->to_string() + " " + vStr(*instr.getVar()) + "\n";
        }

        static std::string generate(const AssignInstruction& instr) {
            // %dst <- t
            return "\t" + vStr(*instr.getDst()) + " <- " + tStr(*instr.getSrc()) + "\n";
        }

        static std::string generate(const OpInstruction& instr) {
            // %dst <- t op t
            return "\t" + vStr(*instr.getDst()) + " <- " +
                   tStr(*instr.getLhs()) + " " + opToString(*instr.getOp()) + " " +
                   tStr(*instr.getRhs()) + "\n";
        }

        static std::string generate(const ArrayLoadInstruction& instr) {
            // %dst <- %src[i]...
            std::string subs;
            for (const auto& ix : instr.getIndices())
                subs += "[" + tStr(ix) + "]";
            return "\t" + vStr(*instr.getDst()) + " <- " + vStr(*instr.getSrc()) + subs + "\n";
        }

        static std::string generate(const ArrayStoreInstruction& instr) {
            // %dst[i]... <- t
            std::string subs;
            for (const auto& ix : instr.getIndices())
                subs += "[" + tStr(ix) + "]";
            return "\t" + vStr(*instr.getDst()) + subs + " <- " + tStr(*instr.getSrc()) + "\n";
        }

        static std::string generate(const LengthInstruction& instr) {
            // %dst <- length %array [dim]
            std::string out = "\t" + vStr(*instr.getDst()) + " <- length " + vStr(*instr.getArray());
            if (instr.getDim().has_value())
                out += " " + tStr(*instr.getDim());
            return out + "\n";
        }

        static std::string generate(const NewArrayInstruction& instr) {
            // %dst <- new Array(args)
            std::string out = "\t" + vStr(*instr.getDst()) + " <- new Array(";
            const auto& args = instr.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) out += ", ";
                out += tStr(args[i]);
            }
            return out + ")\n";
        }

        static std::string generate(const NewTupleInstruction& instr) {
            // %dst <- new Tuple(t)
            return "\t" + vStr(*instr.getDst()) + " <- new Tuple(" + tStr(*instr.getSize()) + ")\n";
        }

        static std::string generate(const VarCallInstruction& instr) {
            // %dst <- @callee(args)   (callee is a function name)
            std::string out = "\t" + vStr(*instr.getDst()) + " <- " +
                              callStr(*instr.getCallee()) + "(";
            const auto& args = instr.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) out += ", ";
                out += tStr(args[i]);
            }
            return out + ")\n";
        }

        static std::string generate(const CallInstruction& instr) {
            // @callee(args)
            std::string out = "\tcall " + callStr(*instr.getCallee()) + "(";
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

        static std::string generate(const BrInstruction& instr) {
            return "\tbr " + lStr(*instr.getTarget()) + "\n";
        }

        static std::string generate(const BrTInstruction& instr) {
            return "\tbr " + tStr(*instr.getCond()) + " " +
                   lStr(*instr.getTrueTarget()) + " " +
                   lStr(*instr.getFalseTarget()) + "\n";
        }

        static std::string generate(const LabelInstruction& instr) {
            return "\t" + lStr(*instr.getLabel()) + "\n";
        }

        // RawInstruction: verbatim final IR text.
        static std::string generate(const RawInstruction& instr) {
            return instr.getText();
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
                case InstructionType::Br:
                    return generate(static_cast<const BrInstruction&>(instr));
                case InstructionType::BrT:
                    return generate(static_cast<const BrTInstruction&>(instr));
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

    private:
        // A callee in IR: user functions are "@name"; the runtime callees
        // (print, input, tensor-error, tuple-error) are bare names per the
        // grammar (callee ::= u | print | input | tuple-error | tensor-error).
        static std::string callStr(const FunctionName& f) {
            const std::string& n = f.name;
            if (n == "print" || n == "input" ||
                n == "tensor-error" || n == "tuple-error")
                return n;
            return "@" + n;
        }
    };

    // Compiler pass entry points.
    void encode_decode_program(Program& p);   // encode.cpp
    void check_accesses(Program& p);     // check.cpp
    void generate_code(Program& p);      // generate.cpp
    void organize_functions(Program& p);

}