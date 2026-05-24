#include "la.h"
#include "ast_leaves.h"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <stdexcept>
#include <cassert>

namespace LA {

    // ============================================================
    // check.cpp  —  Checking array/tensor/tuple accesses
    //
    // For every access of an array/tensor/tuple (ArrayLoad / ArrayStore)
    // we generate, *before* the access:
    //
    //   1. Allocation check  (slides "Checking array/tensor/tuple allocation")
    //        - if v == 0  -> tensor-error(line)
    //
    //   2. Index bounds check for each index i at dimension d
    //      (slides "Checking single-dimension / tensor accesses")
    //        - if i < 0          -> tensor-error(line[, d], length, index)
    //        - if i >= length d  -> tensor-error(line[, d], length, index)
    //
    // Single-dimension array/tuple  -> tensor-error(line, length, index)
    // Multi-dimension tensor        -> tensor-error(line, d, length, index)
    //
    // Anything that is not expressible as a native LA instruction
    // (the tensor-error calls) is emitted as a RawInstruction.
    // ============================================================


    static int64_t checkFreshCounter = 0;

    static Variable checkFreshVar() {
        return Variable("checkVar_" + std::to_string(checkFreshCounter++));
    }

    static Label checkFreshLabel(const std::string& tag) {
        Label l;
        l.name = ":checkLabel_" + tag + "_" + std::to_string(checkFreshCounter++);
        return l;
    }

    // Pretty-print a T (Variable or Number) for use inside a RawInstruction.
    static std::string tToStr(const T& t) {
        return std::visit([](const auto& x) { return x.to_string(); }, t);
    }


    // ------------------------------------------------------------
    // Emit a "fresh declaration" into the declaration bucket and a
    // typed int64 declaration for a freshly created temp.
    // ------------------------------------------------------------
    static void declareInt64(std::vector<std::unique_ptr<Instruction>>& decls,
                             const Variable& v) {
        auto d = std::make_unique<DeclInstruction>();
        d->setVar(v);
        d->setType(Type(VarType::Int64));
        decls.push_back(std::move(d));
    }


    // ------------------------------------------------------------
    // Allocation check:
    //
    //     %cond <- v = 0
    //     br %cond :ERR :OK
    //   :ERR
    //     tensor-error(line)
    //   :OK
    //
    // Emits into `out`; declares %cond into `decls`.
    // ------------------------------------------------------------
    static void emitAllocationCheck(std::vector<std::unique_ptr<Instruction>>& out,
                                    std::vector<std::unique_ptr<Instruction>>& decls,
                                    const Variable& arr,
                                    int64_t lineNumber) {

        Variable cond = checkFreshVar();
        declareInt64(decls, cond);

        Label errL = checkFreshLabel("alloc_err");
        Label okL  = checkFreshLabel("alloc_ok");

        // %cond <- arr = 0
        auto cmp = std::make_unique<OpInstruction>();
        cmp->setDst(cond);
        cmp->setLhs(arr);
        cmp->setOp(Op::Eq);
        cmp->setRhs(Number(0));
        out.push_back(std::move(cmp));

        // br %cond :ERR :OK
        auto br = std::make_unique<BrTInstruction>();
        br->setCond(cond);
        br->setTrueTarget(errL);
        br->setFalseTarget(okL);
        out.push_back(std::move(br));

        // :ERR
        auto errLabel = std::make_unique<LabelInstruction>();
        errLabel->setLabel(errL);
        out.push_back(std::move(errLabel));

        // tensor-error(line)   -- raw IR (not a native LA instruction)
        out.push_back(std::make_unique<RawInstruction>(
            "\tcall @tensor-error(" + std::to_string(lineNumber) + ")\n"));

        // :OK
        auto okLabel = std::make_unique<LabelInstruction>();
        okLabel->setLabel(okL);
        out.push_back(std::move(okLabel));
    }


    // ------------------------------------------------------------
    // Bounds check for a single index `idx` at dimension `dim` of `arr`.
    //
    //     l_d  <- length arr <dim>          ; length of this dimension
    //
    //     ; A. i >= 0
    //     %negCond <- idx < 0
    //     br %negCond :NEG_ERR :NEG_OK
    //   :NEG_ERR
    //     tensor-error(line[, dim], l_d, idx)
    //   :NEG_OK
    //
    //     ; B. i < length
    //     %ltCond <- idx < l_d
    //     br %ltCond :LT_OK :LT_ERR        ; in-range when idx < l_d
    //   :LT_ERR
    //     tensor-error(line[, dim], l_d, idx)
    //   :LT_OK
    //
    // `isTensor` selects the 3-arg vs 4-arg tensor-error signature.
    // ------------------------------------------------------------
    static void emitBoundsCheck(std::vector<std::unique_ptr<Instruction>>& out,
                                std::vector<std::unique_ptr<Instruction>>& decls,
                                const Variable& arr,
                                const T& idx,
                                int64_t dim,
                                bool isTensor,
                                int64_t lineNumber) {

        // l_d <- length arr <dim>
        Variable lenVar = checkFreshVar();
        declareInt64(decls, lenVar);

        auto lenIns = std::make_unique<LengthInstruction>();
        lenIns->setDst(lenVar);
        lenIns->setArray(arr);
        lenIns->setDim(Number(dim));     // 2nd parameter of length stays RAW (a dimension)
        out.push_back(std::move(lenIns));

        // The tensor-error argument list, shared by both failure paths.
        // single-dim : tensor-error(line, length, index)
        // tensor     : tensor-error(line, dim, length, index)
        auto errorCall = [&]() -> std::unique_ptr<Instruction> {
            std::string args = std::to_string(lineNumber);
            if (isTensor) args += ", " + std::to_string(dim);
            args += ", " + lenVar.to_string();
            args += ", " + tToStr(idx);
            return std::make_unique<RawInstruction>(
                "\tcall @tensor-error(" + args + ")\n");
        };

        // ---------- A. Check i is not negative (i >= 0) ----------
        {
            Variable negCond = checkFreshVar();
            declareInt64(decls, negCond);

            Label errL = checkFreshLabel("neg_err");
            Label okL  = checkFreshLabel("neg_ok");

            // %negCond <- idx < 0
            auto cmp = std::make_unique<OpInstruction>();
            cmp->setDst(negCond);
            cmp->setLhs(idx);
            cmp->setOp(Op::Lt);
            cmp->setRhs(Number(0));
            out.push_back(std::move(cmp));

            // br %negCond :ERR :OK   (negative -> error)
            auto br = std::make_unique<BrTInstruction>();
            br->setCond(negCond);
            br->setTrueTarget(errL);
            br->setFalseTarget(okL);
            out.push_back(std::move(br));

            auto errLabel = std::make_unique<LabelInstruction>();
            errLabel->setLabel(errL);
            out.push_back(std::move(errLabel));

            out.push_back(errorCall());

            auto okLabel = std::make_unique<LabelInstruction>();
            okLabel->setLabel(okL);
            out.push_back(std::move(okLabel));
        }

        // ---------- B. Check i < length ----------
        {
            Variable ltCond = checkFreshVar();
            declareInt64(decls, ltCond);

            Label okL  = checkFreshLabel("lt_ok");
            Label errL = checkFreshLabel("lt_err");

            // %ltCond <- idx < l_d
            auto cmp = std::make_unique<OpInstruction>();
            cmp->setDst(ltCond);
            cmp->setLhs(idx);
            cmp->setOp(Op::Lt);
            cmp->setRhs(lenVar);
            out.push_back(std::move(cmp));

            // br %ltCond :OK :ERR   (in range when idx < length)
            auto br = std::make_unique<BrTInstruction>();
            br->setCond(ltCond);
            br->setTrueTarget(okL);
            br->setFalseTarget(errL);
            out.push_back(std::move(br));

            auto errLabel = std::make_unique<LabelInstruction>();
            errLabel->setLabel(errL);
            out.push_back(std::move(errLabel));

            out.push_back(errorCall());

            auto okLabel = std::make_unique<LabelInstruction>();
            okLabel->setLabel(okL);
            out.push_back(std::move(okLabel));
        }
    }


    // ------------------------------------------------------------
    // Emit all checks for one access instruction (ArrayLoad/ArrayStore).
    //   - one allocation check on the accessed variable
    //   - one bounds check per index (dimension = index position)
    // isTensor is true when there is more than one index.
    // ------------------------------------------------------------
    static void emitAccessChecks(std::vector<std::unique_ptr<Instruction>>& out,
                                 std::vector<std::unique_ptr<Instruction>>& decls,
                                 const Variable& arr,
                                 const std::vector<T>& indices,
                                 int64_t lineNumber) {

        // 1. allocation check
        emitAllocationCheck(out, decls, arr, lineNumber);

        // 2. per-index bounds checks
        bool isTensor = indices.size() > 1;
        for (int64_t d = 0; d < static_cast<int64_t>(indices.size()); ++d) {
            emitBoundsCheck(out, decls, arr, indices[d], d, isTensor, lineNumber);
        }
    }


    // ============================================================
    // Entry point: insert access checks across the whole program.
    //
    // Follows encode.cpp's structure: build a newInstructions vector,
    // pushing generated check code before each access, then splice the
    // freshly created declarations into the first basic block.
    // ============================================================
    void check_accesses(Program& p) {
        for (auto& f : p.functions) {

            std::vector<std::unique_ptr<Instruction>> newInstructions;
            std::vector<std::unique_ptr<Instruction>> firstBlockInstructions; // fresh decls

            for (auto& ins : f.instructions) {

                // NOTE: the real line number comes from the parser
                // (in.position().line, see slides). Plumb it onto the
                // instruction and read it here. Placeholder for now:
                int64_t lineNumber = 0;

                switch (ins->type) {

                    // name <- name([t])+   : the accessed source array is `src`
                    case InstructionType::ArrayLoad: {
                        auto* a = dynamic_cast<ArrayLoadInstruction*>(ins.get());
                        assert(a);

                        emitAccessChecks(newInstructions,
                                         firstBlockInstructions,
                                         *a->getSrc(),
                                         a->getIndices(),
                                         lineNumber);

                        // the access itself follows the checks unchanged
                        newInstructions.push_back(std::move(ins));
                        break;
                    }

                    // name([t])+ <- t      : the accessed destination array is `dst`
                    case InstructionType::ArrayStore: {
                        auto* a = dynamic_cast<ArrayStoreInstruction*>(ins.get());
                        assert(a);

                        emitAccessChecks(newInstructions,
                                         firstBlockInstructions,
                                         *a->getDst(),
                                         a->getIndices(),
                                         lineNumber);

                        newInstructions.push_back(std::move(ins));
                        break;
                    }

                    // everything else: pass through unchanged
                    default:
                        newInstructions.push_back(std::move(ins));
                        break;
                }
            }

            // ----- splice fresh declarations into the first basic block -----
            // (same approach as encode.cpp: keep a leading label, then dump
            //  the generated declarations, then the rest of the body.)
            std::vector<std::unique_ptr<Instruction>> finalInstructions;

            auto bodyBegin = newInstructions.begin();

            if (bodyBegin != newInstructions.end() &&
                (*bodyBegin)->type == InstructionType::Label) {
                finalInstructions.push_back(std::move(*bodyBegin));
                ++bodyBegin;
            }

            for (auto& d : firstBlockInstructions)
                finalInstructions.push_back(std::move(d));

            for (auto it = bodyBegin; it != newInstructions.end(); ++it)
                finalInstructions.push_back(std::move(*it));

            f.instructions = std::move(finalInstructions);
        }
    }

}