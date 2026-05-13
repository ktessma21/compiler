#pragma once

#include <IR.h>
#include <ast_leaves.h>

namespace IR {

    class CodeGenerator {

    private:
    // Computes the byte address of base[i0][i1]...[i_{k-1}].
    // Emits the body-base computation (cached per base) and the offset math.
    // Returns the emitted code and the name of the variable holding the final address.
    static std::pair<std::string, std::string>
        compute_index_address(const Variable& base,
                            const std::vector<T>& indices)
        {
            assert(currentFunction);

            const std::string base_name = base.name;
            const std::string base_str  = base.to_string();
            const std::string fn_name   = currentFunction->getName();

            auto it = currentFunction->varTypes.find(base_name);
            assert(it != currentFunction->varTypes.end()
                && "Index op base must have a declared type");
            Type& type = it->second;
            assert(type.kind == TypeKind::Int64
                && "Index op base must be int64[]...");
            assert(type.dim_sizes.size() == indices.size()
                && "Index count must match base's dimension count");
            const size_t ndim = indices.size();

            std::string out;

            // Body base = base + 8 (size slot) + ndim * 8 (dimension slots)
            const std::string body_base = "%" + base_name + "_" + fn_name + "_bodybase";

            if (currentFunction->InitVariables.find(body_base)
                == currentFunction->InitVariables.end()) {
                currentFunction->InitVariables.insert(body_base);

                std::string o1 = currentFunction->fresh(base_name, "o1");
                std::string o2 = currentFunction->fresh(base_name, "o2");
                std::string o  = currentFunction->fresh(base_name, "o");

                out += "\t" + o1 + " <- 8\n";
                out += "\t" + o2 + " <- " + std::to_string(ndim) + " * 8\n";
                out += "\t" + o  + " <- " + o1 + " + " + o2 + "\n";
                out += "\t" + body_base + " <- " + base_str + " + " + o + "\n";
            }

            // Accumulate into %lin:
            std::string lin = currentFunction->fresh(base_name, "lin");
            out += "\t" + lin + " <- 0\n";

            // Build strides (in decoded element units) lazily as we sweep right-to-left.
            // stride_var holds the current "stride for the dimension we're about to add."
            std::string stride_var;   // empty means stride == 1
            for (size_t i = ndim; i-- > 0; ) {
                const std::string idx = tStr(indices[i]);

                // term = idx * stride   (if stride == 1, term = idx)
                std::string term;
                if (stride_var.empty()) {
                    term = idx;
                } else {
                    term = currentFunction->fresh(base_name, "term");
                    out += "\t" + term + " <- " + idx + " * " + stride_var + "\n";
                }
                out += "\t" + lin + " <- " + lin + " + " + term + "\n";

                // Update stride for next (more-significant) dimension:
                // new_stride = stride * dim_sizes[i]
                if (i > 0) {
                    const std::string& d = type.dim_sizes[i];
                    if (stride_var.empty()) {
                        stride_var = d;
                    } else {
                        std::string ns = currentFunction->fresh(base_name, "stride");
                        out += "\t" + ns + " <- " + stride_var + " * " + d + "\n";
                        stride_var = ns;
                    }
                }
            }

            // Convert element offset to byte offset and compute address.
            std::string byte_off = currentFunction->fresh(base_name, "boff");
            out += "\t" + byte_off + " <- " + lin + " * 8\n";

            std::string addr = currentFunction->fresh(base_name, "a");
            out += "\t" + addr + " <- " + body_base + " + " + byte_off + "\n";

            return {out, addr};
        }


    public:

        static inline Function* currentFunction = nullptr;

        static std::string tStr(const T& v){
            return std::visit([](const auto& x) { return x.to_string(); }, v);
        }

        static std::string sStr(const S& v){
            return std::visit([](const auto& x) -> std::string {
                    using V = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<V, Label>)             return ":" + x.name;
                    else if constexpr (std::is_same_v<V, FunctionName>) return "@" + x.name;
                    else                                                return x.to_string();
                }, v);
        }

        // probably don't do anything let's handle it in the backend. because we want to control the traces. 
        
        static std::string generate(const BrInstruction& instr)                { return ""; }
        static std::string generate(const BrTInstruction& instr)               { return ""; }


        static std::string generate(const AssignInstruction& instr)      { return instr.to_string(); }
        static std::string generate(const OpInstruction& instr)          { return instr.to_string(); }
        static std::string generate(const VarCallInstruction& instr)     { return instr.to_string(); }
        static std::string generate(const CallInstruction& instr)        { return instr.to_string(); }
        static std::string generate(const ReturnInstruction& instr)      { return instr.to_string(); }
        static std::string generate(const ReturnTInstruction& instr)     { return instr.to_string(); }
        static std::string generate(const LabelInstruction& instr)       { return instr.to_string(); }
        
        
       static std::string generate(const LengthInstruction& instr) {
            assert(instr.verify());
            assert(currentFunction);

            const std::string dst       = instr.getDst().value().to_string();
            const Variable&   base      = instr.getBase().value();
            const std::string base_name = base.name;
            const std::string base_str  = base.to_string();

            std::string out;

            if (instr.getDim().has_value()) {
                // Address: base + 8 + dim*8
                const std::string dim_str = tStr(instr.getDim().value());

                std::string off    = currentFunction->fresh(base_name, "off");
                std::string dimoff = currentFunction->fresh(base_name, "dimoff");
                std::string addr   = currentFunction->fresh(base_name, "addr");

                out += "\t" + off    + " <- 8\n";
                out += "\t" + dimoff + " <- " + dim_str + " * 8\n";
                out += "\t" + off    + " <- " + off + " + " + dimoff + "\n";
                out += "\t" + addr   + " <- " + base_str + " + " + off + "\n";
                out += "\t" + dst    + " <- load " + addr + "\n";
            } else {
                // Tuple form:  %dst <- length %base
                // Length lives in slot 0. but still when it returns we must encode it . 
                out += "\t" + dst + " <- load " + base_str + "\n";
                out += "\t" + dst + " <- " + dst + " << 1\n";
                out += "\t" + dst + " <- " + dst + " + 1\n";

            }

            return out;
        }
        

        // same as the L3 code. 
        static std::string generate(const NewTupleInstruction& instr)
            {   
                assert(instr.verify());
                assert(currentFunction);

                const std::string dst       = instr.getDst().value().to_string();
                const std::string dst_name  = instr.getDst().value().name;
                const std::string fn_name   = currentFunction->getName();
                const T& size = instr.getSize().value();

                auto argStr = [](const T& v) {
                    return std::visit([](const auto& x) { return x.to_string(); }, v);
                };
                
                std::string out;

                out += "\t" + dst + " <- call allocate(" + argStr(size) + ", 1)\n";
                
                return out; 
            }
        
        
        

        static std::string generate(const IndexStoreInstruction& instr) {
            assert(instr.verify());
            assert(currentFunction);

            const Variable&   base      = instr.getBase().value();
            const std::string base_name = base.name;
            const std::string base_str  = base.to_string();
            const auto&       indices   = instr.getIndices();

            // Look up base's type to choose the indexing path.
            auto it = currentFunction->varTypes.find(base_name);
            assert(it != currentFunction->varTypes.end()
                && "IndexStore base must have a declared type");
            Type& type = it->second;

            

            // ---- Tuple path ----
            if (type.kind == TypeKind::Tuple) {
                std::string out;
                assert(indices.size() == 1
                    && "Tuple indexing takes exactly one index");

                const std::string idx_str = tStr(indices[0]);
                std::string off  = currentFunction->fresh(base_name, "off");
                std::string addr = currentFunction->fresh(base_name, "addr");

                out += "\t" + off  + " <- " + idx_str + " * 8\n";
                out += "\t" + off  + " <- " + off + " + 8\n";
                out += "\t" + addr + " <- " + base_str + " + " + off + "\n";
                out += "\tstore " + addr + " <- " + sStr(instr.getSrc().value()) + "\n";
                return out;
            }

            // ---- Array path ----
            assert(type.kind == TypeKind::Int64
                && "IndexStore base must be int64[]... or tuple");
            assert(type.dim_sizes.size() == indices.size()
                && "Index count must match base's dimension count");


            auto [out, addr] = compute_index_address(instr.getBase().value(),
                                                    instr.getIndices());
            out += "\tstore " + addr + " <- " + sStr(instr.getSrc().value()) + "\n";
            return out;
        }

        static std::string generate(const IndexLoadInstruction& instr) {
            assert(instr.verify());
            assert(currentFunction);

            const Variable&   base      = instr.getBase().value();
            const std::string base_name = base.name;
            const std::string base_str  = base.to_string();
            const auto&       indices   = instr.getIndices();

            auto it = currentFunction->varTypes.find(base_name);
            assert(it != currentFunction->varTypes.end()
                && "IndexStore base must have a declared type");
            Type& type = it->second;

            

            // ---- Tuple path ----
            if (type.kind == TypeKind::Tuple) {
                std::string out;
                assert(indices.size() == 1
                    && "Tuple indexing takes exactly one index");

                const std::string idx_str = tStr(indices[0]);
                std::string off  = currentFunction->fresh(base_name, "off");
                std::string addr = currentFunction->fresh(base_name, "addr");
                const std::string dst = instr.getDst().value().to_string();

                out += "\t" + off  + " <- " + idx_str + " * 8\n";
                out += "\t" + off  + " <- " + off + " + 8\n";
                out += "\t" + addr + " <- " + base_str + " + " + off + "\n";
                out += '\t' + dst + " <- load " + addr + '\n';
                return out;
            }

            // ---- Array path ----
            assert(type.kind == TypeKind::Int64
                && "IndexStore base must be int64[]... or tuple");
            assert(type.dim_sizes.size() == indices.size()
                && "Index count must match base's dimension count");

            const size_t ndim = indices.size();

            auto [out, addr] = compute_index_address(instr.getBase().value(),
                                                    instr.getIndices());
            const std::string dst = instr.getDst().value().to_string();
            out += "\t" + dst + " <- load " + addr + "\n";
            return out;
        }


        static std::string generate(const TypeDeclInstruction& instr) {
            assert(instr.verify());
            assert(currentFunction);

            const Variable& var = instr.getVar().value();
            const Type& type    = instr.getType().value();

            currentFunction->varTypes[var.name] = type;
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

            auto argStr = [](const T& v) {
                return std::visit([](const auto& x) { return x.to_string(); }, v);
            };

            std::string out;

            // 1. Decode each arg:  %pNd <- %pN >> 1
            std::vector<std::string> decoded;
            decoded.reserve(k);
            for (const auto& a : args) {
                std::string d = currentFunction->fresh(dst_name, "d");
                out += "\t" + d + " <- " + argStr(a) + " >> 1\n";
                decoded.push_back(d);
            }

            // record decoded dimension sizes into the destination's type, so later 
            // IndexLoad /IndexStore can use them.

            {
                auto it = currentFunction->varTypes.find(dst_name);
                assert(it != currentFunction->varTypes.end()
                    && "NewArray destination must have a declared type");
                Type& type = it->second;
                assert(type.kind == TypeKind::Int64
                    && "NewArray destination must be int64[]...");
                assert(type.dim_sizes.size() == decoded.size()
                    && "Type's declared dims must match new Array args");
                for (size_t i = 0; i < decoded.size(); ++i) {
                    type.dim_sizes[i] = decoded[i];
                }
            }

            // 2. Multiply all decoded dims pairwise into a single body-size variable.
            std::string size = currentFunction->fresh(dst_name, "size");
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
                std::string slot = currentFunction->fresh(dst_name, "slot");
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