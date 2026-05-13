#pragma once

#include <IR.h>
#include <ast_leaves.h>

namespace IR {

    class CodeGenerator {
    public:

        static inline Function* currentFunction = nullptr;


        static std::string generate(const AssignInstruction& instr)      { return instr.to_string(); }
        static std::string generate(const OpInstruction& instr)          { return instr.to_string(); }
        static std::string generate(const VarCallInstruction& instr)     { return instr.to_string(); }
        static std::string generate(const CallInstruction& instr)        { return instr.to_string(); }
        static std::string generate(const ReturnInstruction& instr)      { return instr.to_string(); }
        static std::string generate(const ReturnTInstruction& instr)     { return instr.to_string(); }
        static std::string generate(const LabelInstruction& instr)       { return instr.to_string(); }
        static std::string generate(const IndexLoadInstruction& instr)         { return ""; }
        
        static std::string generate(const LengthInstruction& instr)            { return ""; }
        
        static std::string generate(const NewTupleInstruction& instr)          { return ""; }
        
        static std::string generate(const BrInstruction& instr)                { return ""; }
        static std::string generate(const BrTInstruction& instr)               { return ""; }
        


        static std::string generate(const IndexStoreInstruction& instr){ 
            assert(instr.verify());
            assert(currentFunction);
            
            
            
            
            
            
            
            
            return ""; }
        static std::string generate(const TypeDeclInstruction& instr) {
            assert(instr.verify());
            assert(currentFunction);

            const Variable& var = instr.getVar().value();
            const Type& type    = instr.getType().value();

            currentFunction->varTypes[var] = type;
            return "";
        }


        static std::string generate(const NewArrayInstruction& instr) {
            assert(instr.verify());
            assert(currentFunction);

            const std::string dst       = instr.getDst().value().to_string();
            const std::string dst_name  = instr.getDst().value().name;
            const std::string fn_name   = currentFunction->getName();
            const auto& args            = instr.getArgs();
            const size_t k              = args.size();

            static int64_t counter = 0;

            // Helper to mint a fresh variable name unique to this dst/function.
            auto fresh = [&](const std::string& tag) {
                return "%" + dst_name + "_" + fn_name + "_" + tag + "_" +
                    std::to_string(counter++);
            };

            auto argStr = [](const T& v) {
                return std::visit([](const auto& x) { return x.to_string(); }, v);
            };

            std::string out;

            // 1. Decode each arg:  %pNd <- %pN >> 1
            std::vector<std::string> decoded;
            decoded.reserve(k);
            for (const auto& a : args) {
                std::string d = fresh("d");
                out += "\t" + d + " <- " + argStr(a) + " >> 1\n";
                decoded.push_back(d);
            }

            // 2. Multiply all decoded dims pairwise into a single body-size variable.
            std::string size = fresh("size");
            out += "\t" + size + " <- " + decoded[0] + "\n";
            for (size_t i = 1; i < k; ++i) {
                out += "\t" + size + " <- " + size + " * " + decoded[i] + "\n";
            }

            // 3. Add k (number of dimensions) for the header slots.
            out += "\t" + size + " <- " + size + " + " + std::to_string(k) + "\n";

            // 4. Encode:  << 1 then + 1
            out += "\t" + size + " <- " + size + " << 1\n";
            out += "\t" + size + " <- " + size + " + 1\n";

            // 5. Call allocate with the encoded size and init value 1.
            out += "\t" + dst + " <- call allocate(" + size + ", 1)\n";

            // 6. Store each original (encoded) param at the dimension slots.
            //    Slot 0 holds the size (set by allocate); dimensions go at +8, +16, ...
            for (size_t i = 0; i < k; ++i) {
                std::string slot = fresh("slot");
                const int64_t offset = static_cast<int64_t>((i + 1) * 8);
                out += "\t" + slot + " <- " + dst + " + " + std::to_string(offset) + "\n";
                out += "\tstore " + slot + " <- " + argStr(args[i]) + "\n";
            }

            return out;
        }




        static std::string generate(const Instruction& instr) {
            switch (instr.type) {
                case InstructionType::AssignFromS:
                    return generate(static_cast<const AssignInstruction&>(instr));
                case InstructionType::AssignFromOp:
                    return generate(static_cast<const OpInstruction&>(instr));
                case InstructionType::AssignFromIndex:
                    return generate(static_cast<const IndexLoadInstruction&>(instr));
                case InstructionType::StoreIndex:
                    return generate(static_cast<const IndexStoreInstruction&>(instr));
                case InstructionType::AssignFromLength:
                    return generate(static_cast<const LengthInstruction&>(instr));
                case InstructionType::AssignFromCall:
                    return generate(static_cast<const VarCallInstruction&>(instr));
                case InstructionType::Call:
                    return generate(static_cast<const CallInstruction&>(instr));
                case InstructionType::AssignFromNewArray:
                    return generate(static_cast<const NewArrayInstruction&>(instr));
                case InstructionType::AssignFromNewTuple:
                    return generate(static_cast<const NewTupleInstruction&>(instr));
                case InstructionType::TypeDecl:
                    return generate(static_cast<const TypeDeclInstruction&>(instr));
                case InstructionType::Br:
                    return generate(static_cast<const BrInstruction&>(instr));
                case InstructionType::BrT:
                    return generate(static_cast<const BrTInstruction&>(instr));
                case InstructionType::Return:
                    return generate(static_cast<const ReturnInstruction&>(instr));
                case InstructionType::ReturnT:
                    return generate(static_cast<const ReturnTInstruction&>(instr));
                case InstructionType::Label:
                    return generate(static_cast<const LabelInstruction&>(instr));
                case InstructionType::Unknown:
                default:
                    throw std::runtime_error("generate: unhandled instruction type "
                                             + std::to_string(static_cast<int>(instr.type)));
            }
        }

        static inline std::string OptoString(Op o) {
            switch (o) {
                case Op::Add: return "+";
                case Op::Sub: return "-";
                case Op::Mul: return "*";
                case Op::And: return "&";
                case Op::Shl: return "<<";
                case Op::Shr: return ">>";
                case Op::Lt:  return "<";
                case Op::Le:  return "<=";
                case Op::Eq:  return "=";
                case Op::Ge:  return ">=";
                case Op::Gt:  return ">";
            }
            throw std::runtime_error("OptoString: unknown Op");
        }
    };

    void generate_code(const Program& p);
}