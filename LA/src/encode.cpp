#include "la.h"
#include "ast_leaves.h"
#include "generate.h"
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <stdexcept>
#include <cassert>

namespace LA {

    static bool debug(){
    if (std::getenv("LA_DEBUG") != nullptr) {
        return true;
    }
    return false;
}



    static int64_t freshCounter = 0;
 
    static Variable freshVar() {
        return Variable("decodeVar_" + std::to_string(freshCounter++));
    }

    static bool isVariable(const T& t) { return std::holds_alternative<Variable>(t); }



    // encode(x) = (x << 1) | 1  
    static inline int64_t encodeConst(int64_t x) {
        return (x << 1) | 1;
    }

    // If the slot holds a Number, replace it with its encoded value.
    // Variables are left untouched (they are handled at runtime in Step 2).
    static void encodeSlot(T& slot) {
        if (auto* n = std::get_if<Number>(&slot)) {
            assert(std::get<Number>(slot).getValue() != encodeConst(n->getValue()));
            // if (debug){
            //     std::cerr << "Encoding constant: " << n->getValue() << " -> " << encodeConst(n->getValue()) << std::endl;
            // }
            slot = T(Number(encodeConst(n->getValue())));
            // if (debug){
            //     std::cerr << "Encoded constant: " << std::get<Number>(slot).getValue() << std::endl;
            // }
        }
    }

    // Rewrite every encodable constant operand of a single instruction.
    static void encodeConstantsIn(Instruction* ins) {
        switch (ins->type) {

            // name <- t          : the source value is encoded
            case InstructionType::AssignFromT: {
                auto* a = static_cast<AssignInstruction*>(ins);
                if (a->getSrc().has_value()) {
                    T v = *a->getSrc();
                    encodeSlot(v);
                    a->setSrc(std::move(v));
                }
                break;
            }

            // name <- t op t     : both operands are encoded
            case InstructionType::AssignFromOp: {
                auto* o = static_cast<OpInstruction*>(ins);
                if (o->getLhs().has_value()) { T v = *o->getLhs(); encodeSlot(v); o->setLhs(std::move(v)); }
                if (o->getRhs().has_value()) { T v = *o->getRhs(); encodeSlot(v); o->setRhs(std::move(v)); }
                break;
            }

            // name([t])+ <- t    : indices stay raw; the stored value is encoded
            case InstructionType::ArrayStore: {
                auto* s = static_cast<ArrayStoreInstruction*>(ins);
                if (s->getSrc().has_value()) {
                    T v = *s->getSrc();
                    encodeSlot(v);
                    s->setSrc(std::move(v));
                }
                // indices: intentionally left raw
                break;
            }

            // return t           : the returned value is encoded
            case InstructionType::ReturnT: {
                auto* r = static_cast<ReturnTInstruction*>(ins);
                if (r->getValue().has_value()) {
                    T v = *r->getValue();
                    encodeSlot(v);
                    r->setValue(std::move(v));
                }
                break;
            }

            // br t label label   : the condition is encoded
            case InstructionType::BrT: {
                auto* b = static_cast<BrTInstruction*>(ins);
                if (b->getCond().has_value()) {
                    T v = *b->getCond();
                    encodeSlot(v);
                    b->setCond(std::move(v));
                }
                break;
            }

            // name ( args )      : every argument is encoded
            case InstructionType::Call: {

                if (debug()){
                    std::cerr << "Encoding CallInstruction: " << ins->to_string() << std::endl;
                }

                std::vector<T> args;
                auto* c = static_cast<CallInstruction*>(ins);

                for (const auto& arg : c->getArgs()) {
                    T v = arg;
                    encodeSlot(v);
                    args.push_back(std::move(v));
                }
                c->getArgs() = std::move(args); 
                break;
            }

            // name <- name(args) : every argument is encoded
            case InstructionType::AssignFromCall: {
                auto* c = static_cast<VarCallInstruction*>(ins);
                std::vector<T> args;
                for (const auto& arg : c->getArgs()) {
                T v = arg;
                encodeSlot(v);
                args.push_back(std::move(v));
            }
                c->getArgs() = std::move(args); 
                break;
            }

            // ----- slots intentionally left RAW (no encoding) -----
            // ArrayLoad : indices are array/tuple indices
            // Length    : 2nd parameter (the dimension) stays raw
            // NewArray  : args are allocation sizes
            // NewTuple  : size is an allocation size
            // Decl / Label / Br / Return / Raw : no constant operands to encode
            case InstructionType::ArrayLoad:
            case InstructionType::Length:
            case InstructionType::NewArray:
            case InstructionType::NewTuple:
            case InstructionType::Decl:
            case InstructionType::Label:
            case InstructionType::Br:
            case InstructionType::Return:
            case InstructionType::Raw:
            case InstructionType::Unknown:
            default:
                break;
        }
    }

    // Step 1 entry point: encode all eligible constants across the program.
    void encode_program(Program& p) {
        for (auto& f : p.functions) {
            for (auto& ins : f.instructions) {
                encodeConstantsIn(ins.get());
            }


            std::vector<std::unique_ptr<Instruction>> newInstructions;
            std::vector<std::unique_ptr<Instruction>> firstBlockInstructions;
            // std::vector<std::unique_ptr<Instruction>> initializationInstructions;

            for (auto& ins : f.instructions) {

                auto decodeVars = ins->toDecode();

                // maps original variable name -> decoded temp
                std::map<std::string, Variable> decodedMap;
                // numbers in toDecode(i) get a decoded temp too; keyed by the
                // (already-encoded) constant value so identical constants share one temp
                std::map<int64_t, Variable> decodedNumMap;

               
                for (const auto& t : decodeVars) {

                    // ---- Variable operand: decode at runtime (original >> 1) ----
                    if (isVariable(t)) {

                        Variable original = std::get<Variable>(t);

                        // already created a temp for this variable in this instruction
                        if (decodedMap.count(original.name)) continue;

                        Variable fresh = freshVar();

                        decodedMap[original.name] = fresh;

                        /*
                        * int64 %tmp
                        */
                        auto decl = std::make_unique<DeclInstruction>();
                        decl->setVar(fresh);
                        decl->setType(Type(VarType::Int64));

                        firstBlockInstructions.push_back(std::move(decl));

                        /*
                        * %tmp <- original >> 1
                        */
                        auto decodeInstr = std::make_unique<OpInstruction>();
                        decodeInstr->setDst(fresh);
                        decodeInstr->setLhs(original);
                        decodeInstr->setOp(Op::Shr);
                        decodeInstr->setRhs(Number(1));
                        decodeInstr->just_decoded = true;

                        newInstructions.push_back(std::move(decodeInstr));
                        continue;
                    }

                    // ---- Number operand: also gets a decoded temp ----
                    // The constant was already encoded in Step 1, so its decoded
                    // value (enc >> 1) is known at compile time. We declare a temp
                    // and initialize it directly to that decoded constant.
                    if (std::holds_alternative<Number>(t)) {

                        int64_t encVal = std::get<Number>(t).getValue();

                        if (decodedNumMap.count(encVal)) continue;

                        Variable fresh = freshVar();
                        decodedNumMap[encVal] = fresh;

                        /*
                        * int64 %tmp
                        */
                        auto decl = std::make_unique<DeclInstruction>();
                        decl->setVar(fresh);
                        decl->setType(Type(VarType::Int64));

                        firstBlockInstructions.push_back(std::move(decl));

                        /*
                        * %tmp <- <decoded constant>
                        */
                        auto init = std::make_unique<AssignInstruction>();
                        init->setDst(fresh);
                        init->setSrc(Number(encVal >> 1));
                        newInstructions.push_back(std::move(init));
                    }
                }

                /*
                * Rewrite instruction once
                */
                switch (ins->type) {

                    case InstructionType::AssignFromOp: {

                        auto* old = dynamic_cast<OpInstruction*>(ins.get());
                        assert(old);

                        auto neo = std::make_unique<OpInstruction>();

                        neo->setDst(*old->getDst());
                        neo->setOp(*old->getOp());
                    

                        auto rewriteT = [&](const T& t) -> T {

                            if (isVariable(t)) {
                                Variable v = std::get<Variable>(t);
                                auto it = decodedMap.find(v.name);
                                if (it != decodedMap.end())
                                    return it->second;
                                return v;
                            }

                            if (std::holds_alternative<Number>(t)) {
                                int64_t encVal = std::get<Number>(t).getValue();
                                auto it = decodedNumMap.find(encVal);
                                if (it != decodedNumMap.end())
                                    return it->second;
                            }

                            return t;
                        };

                        neo->setLhs(rewriteT(*old->getLhs()));
                        neo->setRhs(rewriteT(*old->getRhs()));

                        newInstructions.push_back(std::move(neo));
                        break;
                    }

                    case InstructionType::BrT: {

                        auto* old = dynamic_cast<BrTInstruction*>(ins.get());
                        assert(old);

                        auto neo = std::make_unique<BrTInstruction>();

                        T cond = *old->getCond();

                        if (isVariable(cond)) {

                            Variable v = std::get<Variable>(cond);

                            auto it = decodedMap.find(v.name);

                            if (it != decodedMap.end())
                                cond = it->second;
                        } else if (std::holds_alternative<Number>(cond)) {

                            int64_t encVal = std::get<Number>(cond).getValue();

                            auto it = decodedNumMap.find(encVal);

                            if (it != decodedNumMap.end())
                                cond = it->second;
                        }

                        neo->setCond(cond);
                        neo->setTrueTarget(*old->getTrueTarget());
                        neo->setFalseTarget(*old->getFalseTarget());

                        newInstructions.push_back(std::move(neo));
                        break;
                    }

                    case InstructionType::ArrayLoad: {

                        auto* old = dynamic_cast<ArrayLoadInstruction*>(ins.get());
                        assert(old);

                        auto neo = std::make_unique<ArrayLoadInstruction>();

                        neo->setDst(*old->getDst());
                        neo->setSrc(*old->getSrc());

                        for (const auto& idx : old->getIndices()) {

                            if (isVariable(idx)) {

                                Variable v = std::get<Variable>(idx);

                                auto it = decodedMap.find(v.name);

                                if (it != decodedMap.end()) {
                                    neo->addIndex(it->second);
                                    continue;
                                }
                            } else if (std::holds_alternative<Number>(idx)) {

                                int64_t encVal = std::get<Number>(idx).getValue();

                                auto it = decodedNumMap.find(encVal);

                                if (it != decodedNumMap.end()) {
                                    neo->addIndex(it->second);
                                    continue;
                                }
                            }

                            neo->addIndex(idx);
                        }

                        newInstructions.push_back(std::move(neo));
                        break;
                    }

                    case InstructionType::ArrayStore: {

                        auto* old = dynamic_cast<ArrayStoreInstruction*>(ins.get());
                        assert(old);

                        auto neo = std::make_unique<ArrayStoreInstruction>();

                        neo->setDst(*old->getDst());
                        neo->setSrc(*old->getSrc());

                        for (const auto& idx : old->getIndices()) {

                            if (isVariable(idx)) {

                                Variable v = std::get<Variable>(idx);

                                auto it = decodedMap.find(v.name);

                                if (it != decodedMap.end()) {
                                    neo->addIndex(it->second);
                                    continue;
                                }
                            } else if (std::holds_alternative<Number>(idx)) {

                                int64_t encVal = std::get<Number>(idx).getValue();

                                auto it = decodedNumMap.find(encVal);

                                if (it != decodedNumMap.end()) {
                                    neo->addIndex(it->second);
                                    continue;
                                }
                            }

                            neo->addIndex(idx);
                        }

                        newInstructions.push_back(std::move(neo));
                        break;
                    }

                    case InstructionType::Length: {

                        auto* old = dynamic_cast<LengthInstruction*>(ins.get());
                        assert(old);

                        auto neo = std::make_unique<LengthInstruction>();

                        neo->setDst(*old->getDst());
                        neo->setArray(*old->getArray());

                        if (old->getDim().has_value()) {

                            T dim = *old->getDim();

                            if (isVariable(dim)) {

                                Variable v = std::get<Variable>(dim);

                                auto it = decodedMap.find(v.name);

                                if (it != decodedMap.end())
                                    dim = it->second;
                            } else if (std::holds_alternative<Number>(dim)) {

                                int64_t encVal = std::get<Number>(dim).getValue();

                                auto it = decodedNumMap.find(encVal);

                                if (it != decodedNumMap.end())
                                    dim = it->second;
                            }

                            neo->setDim(dim);
                        }

                        newInstructions.push_back(std::move(neo));
                        break;
                    }

                    case InstructionType::AssignFromCall: {

                        auto* old = dynamic_cast<VarCallInstruction*>(ins.get());
                        assert(old);

                        auto neo = std::make_unique<VarCallInstruction>();

                        neo->setDst(*old->getDst());
                        neo->setCallee(*old->getCallee());

                        for (const auto& arg : old->getArgs()) {

                            T rewritten = arg;

                            if (isVariable(arg)) {

                                Variable v = std::get<Variable>(arg);

                                auto it = decodedMap.find(v.name);

                                if (it != decodedMap.end())
                                    rewritten = it->second;
                            }

                            neo->addArg(rewritten);
                        }

                        newInstructions.push_back(std::move(neo));
                        break;
                    }

                    // type name : move the declaration to the first basic block
                    // (per slide: collect all LA decls and emit them at the start
                    //  of the first basic block, in any order)
                    case InstructionType::Decl: {
                        firstBlockInstructions.push_back(std::move(ins));
                        break;
                    }

                    default:
                        newInstructions.push_back(std::move(ins));
                        break;
                }
            }





          
            /// encoding step
            // For every variable v in toEncode(i), encode v just after i:
            //     v <- v << 1
            //     v <- v + 1
            std::vector<std::unique_ptr<Instruction>> encodedInstructions;

            for (auto& ins : newInstructions) {

                // toEncode(i) is non-empty only for "var <- t op t"; for everything
                // else it is empty, so we just pass the instruction through.
                auto encodeVars = ins->toEncode();

                InstructionType insType = ins->type;

                auto* opIns = dynamic_cast<OpInstruction*>(ins.get());

                bool skipEncoding =
                    (insType == InstructionType::AssignFromOp &&
                    opIns &&
                    opIns->just_decoded);

                encodedInstructions.push_back(std::move(ins));

                if (insType != InstructionType::AssignFromOp) {
                    assert(encodeVars.empty());
                    continue;
                }

                if (skipEncoding)
                    continue;

                
                for (const auto& v : encodeVars) {

                    // toEncode() yields Variables directly (the dst of "var <- t op t")

                    /*
                    * v <- v << 1
                    */
                    auto shl = std::make_unique<OpInstruction>();
                    shl->setDst(v);
                    shl->setLhs(v);
                    shl->setOp(Op::Shl);
                    shl->setRhs(Number(1));
                    encodedInstructions.push_back(std::move(shl));

                    /*
                    * v <- v + 1
                    */
                    auto add = std::make_unique<OpInstruction>();
                    add->setDst(v);
                    add->setLhs(v);
                    add->setOp(Op::Add);
                    add->setRhs(Number(1));
                    encodedInstructions.push_back(std::move(add));
                }
            }

            // ----- splice decoded-temp decls + their initialization back in -----
            // Declarations and the "%tmp <- src >> 1" decode instructions are placed
            // at the beginning of the first basic block: first the leading label (if
            // the function starts with one), then the declarations, then the decode
            // initializations, then the rest of the body.
            std::vector<std::unique_ptr<Instruction>> finalInstructions;

            auto bodyBegin = encodedInstructions.begin();

            // keep a leading label at the very top of the function, if present
            if (bodyBegin != encodedInstructions.end() &&
                (*bodyBegin)->type == InstructionType::Label) {
                finalInstructions.push_back(std::move(*bodyBegin));
                ++bodyBegin;
            }

            for (auto& d : firstBlockInstructions)
                finalInstructions.push_back(std::move(d));

       

            for (auto it = bodyBegin; it != encodedInstructions.end(); ++it)
                finalInstructions.push_back(std::move(*it));

            f.instructions = std::move(finalInstructions);
        }
    }

}