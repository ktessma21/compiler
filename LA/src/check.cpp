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
        return std::visit([](const auto& x) -> std::string {
            using U = std::decay_t<decltype(x)>;

            if constexpr (std::is_same_v<U, Variable>) {
                return "%" + x.to_string();
            } else {
                return x.to_string();
            }
        }, t);
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


    // (x << 1) | 1  — same encoding used by encode_decode_program
    static inline int64_t checkEncodeConst(int64_t x) {
        return (x << 1) | 1;
    }

   
    static Variable emitEncodedConst(std::vector<std::unique_ptr<Instruction>>& out,
                                    std::vector<std::unique_ptr<Instruction>>& decls,
                                    int64_t rawValue) {
        Variable v = checkFreshVar();
        declareInt64(decls, v);

        auto a = std::make_unique<AssignInstruction>();
        a->setDst(v);
        a->setSrc(Number(checkEncodeConst(rawValue)));
        out.push_back(std::move(a));

        return v;
    }

    static Variable emitEncodedVar(std::vector<std::unique_ptr<Instruction>>& out,
                                std::vector<std::unique_ptr<Instruction>>& decls,
                                const Variable& src) {
        Variable v = checkFreshVar();
        declareInt64(decls, v);

        auto cp = std::make_unique<AssignInstruction>();
        cp->setDst(v);
        cp->setSrc(src);
        out.push_back(std::move(cp));

        auto shl = std::make_unique<OpInstruction>();
        shl->setDst(v);
        shl->setLhs(v);
        shl->setOp(Op::Shl);
        shl->setRhs(Number(1));
        out.push_back(std::move(shl));

        auto add = std::make_unique<OpInstruction>();
        add->setDst(v);
        add->setLhs(v);
        add->setOp(Op::Add);
        add->setRhs(Number(1));
        out.push_back(std::move(add));

        return v;
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
        auto cmp = std::make_unique<OpInstruction>(lineNumber);
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

        // tensor-error(encoded line)
        Variable lineVar = emitEncodedConst(out, decls, lineNumber);

        auto call = std::make_unique<CallInstruction>();
        call->setCallee(FunctionName("tensor-error"));
        call->addArg(lineVar);
        out.push_back(std::move(call));

        // :OK
        auto okLabel = std::make_unique<LabelInstruction>();
        okLabel->setLabel(okL);
        out.push_back(std::move(okLabel));
    }


    static void emitBoundsCheck(std::vector<std::unique_ptr<Instruction>>& out,
                                std::vector<std::unique_ptr<Instruction>>& decls,
                                const Variable& arr,
                                const T& idx,
                                int64_t dim,
                                bool isTensor,
                                int64_t lineNumber, 
                                Function& fn) {

        // l_d <- length arr <dim>
        
        Variable lenVar = checkFreshVar();
        declareInt64(decls, lenVar);

        auto lenIns = std::make_unique<LengthInstruction>();
        lenIns->setDst(lenVar);
        lenIns->setArray(arr);
        bool isTuple = false;
        auto it = fn.declTypes.find(arr.name);
        if (it != fn.declTypes.end() && it->second == VarType::Tuple)
            isTuple = true;
        if (!isTuple) lenIns->setDim(Number(dim));
        out.push_back(std::move(lenIns));

        // (mirror of instructor's %newVar5 <- %newVar9)
        Variable lenForError = checkFreshVar();
        declareInt64(decls, lenForError);
        {
            auto cp = std::make_unique<AssignInstruction>();
            cp->setDst(lenForError);
            cp->setSrc(lenVar);
            out.push_back(std::move(cp));

            // if ()
            // // %newVar9 <- %newVar9 >> 1
            auto decodeLenVar = std::make_unique<OpInstruction>();
            decodeLenVar->setDst(lenVar);
            decodeLenVar->setLhs(lenVar);
            decodeLenVar->setOp(Op::Shr);
            decodeLenVar->setRhs(Number(1));
            out.push_back(std::move(decodeLenVar));

        }

        // ----- Pre-encode the remaining tensor-error arguments once -----
        Variable encLine = emitEncodedConst(out, decls, lineNumber);          // step 2
        Variable encDim;
        if (isTensor) encDim = emitEncodedConst(out, decls, dim);             // step 2

        // index: Number -> encoded const; Variable -> copy+encode at runtime  (step 3)
        Variable encIdx = std::visit([&](const auto& x) -> Variable {
            using U = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<U, Number>)
                return emitEncodedConst(out, decls, x.getValue());
            else
                return emitEncodedVar(out, decls, x);
        }, idx);

   
        // Step 5: build tensor-error as a real CallInstruction with encoded temps.
        // single-dim : tensor-error(line, length, index)
        // tensor     : tensor-error(line, dim, length, index)
        auto errorCall = [&]() -> std::unique_ptr<Instruction> {
            auto c = std::make_unique<CallInstruction>();
            const char* errName = isTuple ? "tuple-error" : "tensor-error";
            c->setCallee(FunctionName(errName));
            c->addArg(encLine);
            if (isTensor) c->addArg(encDim);
            c->addArg(lenForError);
            c->addArg(encIdx);
            return c;
        };

        // ---------- A. Check i is not negative (i >= 0) ----------
        {
            Variable negCond = checkFreshVar();
            declareInt64(decls, negCond);

            Label errL = checkFreshLabel("neg_err");
            Label okL  = checkFreshLabel("neg_ok");

            // %ltCond <- idx < l_d
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
            cmp->setLhs(idx);             // was: idx
            cmp->setOp(Op::Lt);
            cmp->setRhs(lenVar);             // lenVar is the raw native length — correct
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
                                 int64_t lineNumber, Function& fn) {

        // 1. allocation check
        emitAllocationCheck(out, decls, arr, lineNumber);

        bool isTensor = indices.size() > 1;
        for (int64_t d = 0; d < static_cast<int64_t>(indices.size()); ++d) {
            emitBoundsCheck(out, decls, arr, indices[d], d, isTensor, lineNumber, fn);
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

                int64_t lineNumber = ins->getLineNumber();


                switch (ins->type) {

                    // name <- name([t])+   : the accessed source array is `src`
                    case InstructionType::ArrayLoad: {
                        auto* a = dynamic_cast<ArrayLoadInstruction*>(ins.get());
                        assert(a);

                        emitAccessChecks(newInstructions,
                                         firstBlockInstructions,
                                         *a->getSrc(),
                                         a->getIndices(),
                                         lineNumber, f);

                        // the access itself follows the checks unchanged
                        // for (const auto& indice : a )
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
                                         lineNumber, f);

                       

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
