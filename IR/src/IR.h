#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <cassert>
#include <optional>
#include <list>
#include <type_traits>
#include <set>
#include "ast_leaves.h"


namespace IR {

    // forward declarations
    class Function;
    struct BasicBlock;

    // ============================================================
    // Instruction hierarchy
    // ============================================================
    class Instruction : public ASTNode {
    public:
        InstructionType type;
        Instruction() = delete;
        Instruction(InstructionType t) : type(t) {}
        virtual ~Instruction() = default;

        bool verify() const override { return true; }
        virtual std::string to_string() const = 0;
        virtual std::set<Variable> reads()  const { return {}; }
        virtual std::set<Variable> writes() const { return {}; }
    };


    /* ============================================================
     * Shared helpers for callees / args / s-values / t-values
     * ============================================================ */
    inline std::string calleeToString(const Callee& c) {
        return std::visit([](const auto& x) -> std::string {
            using V = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<V, BuiltinCallee>) return builtinCalleeToString(x);
            else                                            return x.to_string();
        }, c);
    }

    inline std::string tToString(const T& v) {
        return std::visit([](const auto& x) { return x.to_string(); }, v);
    }

    inline std::string sToString(const S& v) {
        return std::visit([](const auto& x) { return x.to_string(); }, v);
    }

    inline std::string argsToString(const std::vector<T>& args) {
        std::string out;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) out += ", ";
            out += tToString(args[i]);
        }
        return out;
    }

    inline std::string indicesToString(const std::vector<T>& idx) {
        std::string out;
        for (const auto& i : idx) out += "[" + tToString(i) + "]";
        return out;
    }


    /* ============================================================
     * TypeDeclInstruction  —  type var
     * ============================================================ */
    class TypeDeclInstruction : public Instruction {
        std::optional<Type>     declType;
        std::optional<Variable> var;
    public:
        TypeDeclInstruction() : Instruction(InstructionType::TypeDecl) {}

        void setType(Type t)    { declType = std::move(t); }
        void setVar(Variable v) { var = std::move(v); }

        const std::optional<Type>&     getType() const { return declType; }
        const std::optional<Variable>& getVar()  const { return var; }

        bool verify() const override { return declType.has_value() && var.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\t" + declType->to_string() + " " + var->to_string() + "\n";
        }
        // a declaration neither reads nor writes for liveness purposes
    };


    /* ============================================================
     * AssignInstruction  —  var <- s
     * ============================================================ */
    class AssignInstruction : public Instruction {
        std::optional<Variable> dst;
        std::optional<S>        src;
    public:
        AssignInstruction() : Instruction(InstructionType::AssignFromS) {}

        void setDst(Variable v) { dst = std::move(v); }
        void setSrc(S s)        { src = std::move(s); }

        const std::optional<Variable>& getDst() const { return dst; }
        const std::optional<S>&        getSrc() const { return src; }

        bool verify() const override { return dst.has_value() && src.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\t" + dst->to_string() + " <- " + sToString(*src) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (!src.has_value()) return r;
            if (auto* v = std::get_if<Variable>(&*src)) r.insert(*v);
            return r;
        }

        std::set<Variable> writes() const override {
            std::set<Variable> w;
            if (dst.has_value()) w.insert(*dst);
            return w;
        }
    };


    /* ============================================================
     * OpInstruction  —  var <- t op t
     *   In IR, op covers both arithmetic and comparisons.
     * ============================================================ */
    class OpInstruction : public Instruction {
        std::optional<Variable> dst;
        std::optional<T>        lhs;
        std::optional<Op>       op;
        std::optional<T>        rhs;
    public:
        OpInstruction() : Instruction(InstructionType::AssignFromOp) {}

        void setDst(Variable v) { dst = std::move(v); }
        void setLhs(T t)        { lhs = std::move(t); }
        void setOp(Op o)        { op  = o; }
        void setRhs(T t)        { rhs = std::move(t); }

        const std::optional<Variable>& getDst() const { return dst; }
        const std::optional<T>&        getLhs() const { return lhs; }
        const std::optional<Op>&       getOp()  const { return op;  }
        const std::optional<T>&        getRhs() const { return rhs; }

        bool verify() const override {
            return dst.has_value() && lhs.has_value() && op.has_value() && rhs.has_value();
        }

        std::string to_string() const override {
            assert(verify());
            return "\t" + dst->to_string() + " <- " +
                   tToString(*lhs) + " " + opToString(*op) + " " + tToString(*rhs) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (lhs.has_value()) if (auto* v = std::get_if<Variable>(&*lhs)) r.insert(*v);
            if (rhs.has_value()) if (auto* v = std::get_if<Variable>(&*rhs)) r.insert(*v);
            return r;
        }

        std::set<Variable> writes() const override {
            std::set<Variable> w;
            if (dst.has_value()) w.insert(*dst);
            return w;
        }
    };


    /* ============================================================
     * IndexLoadInstruction  —  var <- var([t])+
     * ============================================================ */
    class IndexLoadInstruction : public Instruction {
        std::optional<Variable> dst;
        std::optional<Variable> base;
        std::vector<T>          indices;
    public:
        IndexLoadInstruction() : Instruction(InstructionType::AssignFromIndex) {}

        void setDst(Variable v)  { dst  = std::move(v); }
        void setBase(Variable v) { base = std::move(v); }
        void addIndex(T t)       { indices.push_back(std::move(t)); }

        const std::optional<Variable>& getDst()     const { return dst; }
        const std::optional<Variable>& getBase()    const { return base; }
        const std::vector<T>&          getIndices() const { return indices; }

        bool verify() const override {
            return dst.has_value() && base.has_value() && !indices.empty();
        }

        std::string to_string() const override {
            assert(verify());
            return "\t" + dst->to_string() + " <- " +
                   base->to_string() + indicesToString(indices) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (base.has_value()) r.insert(*base);
            for (const auto& i : indices)
                if (auto* v = std::get_if<Variable>(&i)) r.insert(*v);
            return r;
        }

        std::set<Variable> writes() const override {
            std::set<Variable> w;
            if (dst.has_value()) w.insert(*dst);
            return w;
        }
    };


    /* ============================================================
     * IndexStoreInstruction  —  var([t])+ <- s
     * ============================================================ */
    class IndexStoreInstruction : public Instruction {
        std::optional<Variable> base;
        std::vector<T>          indices;
        std::optional<S>        src;
    public:
        IndexStoreInstruction() : Instruction(InstructionType::StoreIndex) {}

        void setBase(Variable v) { base = std::move(v); }
        void addIndex(T t)       { indices.push_back(std::move(t)); }
        void setSrc(S s)         { src = std::move(s); }

        const std::optional<Variable>& getBase()    const { return base; }
        const std::vector<T>&          getIndices() const { return indices; }
        const std::optional<S>&        getSrc()     const { return src; }

        bool verify() const override {
            return base.has_value() && !indices.empty() && src.has_value();
        }

        std::string to_string() const override {
            assert(verify());
            return "\t" + base->to_string() + indicesToString(indices) +
                   " <- " + sToString(*src) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (base.has_value()) r.insert(*base);
            for (const auto& i : indices)
                if (auto* v = std::get_if<Variable>(&i)) r.insert(*v);
            if (src.has_value())
                if (auto* v = std::get_if<Variable>(&*src)) r.insert(*v);
            return r;
        }
        // store-into-index writes to memory through `base`, not to a fresh variable
    };


    /* ============================================================
     * LengthInstruction  —  var <- length var t   |   var <- length var
     *   (one form covers arrays w/ dim arg and tuples w/o)
     * ============================================================ */
    class LengthInstruction : public Instruction {
        std::optional<Variable> dst;
        std::optional<Variable> base;
        std::optional<T>        dim;   // optional: present for arrays
    public:
        LengthInstruction() : Instruction(InstructionType::AssignFromLength) {}

        void setDst(Variable v)  { dst  = std::move(v); }
        void setBase(Variable v) { base = std::move(v); }
        void setDim(T t)         { dim  = std::move(t); }

        const std::optional<Variable>& getDst()  const { return dst; }
        const std::optional<Variable>& getBase() const { return base; }
        const std::optional<T>&        getDim()  const { return dim; }

        bool verify() const override { return dst.has_value() && base.has_value(); }

        std::string to_string() const override {
            assert(verify());
            std::string out = "\t" + dst->to_string() + " <- length " + base->to_string();
            if (dim.has_value()) out += " " + tToString(*dim);
            return out + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (base.has_value()) r.insert(*base);
            if (dim.has_value())
                if (auto* v = std::get_if<Variable>(&*dim)) r.insert(*v);
            return r;
        }

        std::set<Variable> writes() const override {
            std::set<Variable> w;
            if (dst.has_value()) w.insert(*dst);
            return w;
        }
    };


    /* ============================================================
     * VarCallInstruction  —  var <- call callee ( args? )
     * ============================================================ */
    class VarCallInstruction : public Instruction {
        std::optional<Variable> dst;
        std::optional<Callee>   callee;
        std::vector<T>          args;
    public:
        VarCallInstruction() : Instruction(InstructionType::AssignFromCall) {}

        void setDst(Variable v)  { dst = std::move(v); }
        void setCallee(Callee c) { callee = std::move(c); }
        void addArg(T t)         { args.push_back(std::move(t)); }

        const std::optional<Variable>& getDst()    const { return dst; }
        const std::optional<Callee>&   getCallee() const { return callee; }
        const std::vector<T>&          getArgs()   const { return args; }

        bool verify() const override { return dst.has_value() && callee.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\t" + dst->to_string() + " <- call " +
                   calleeToString(*callee) + "(" + argsToString(args) + ")\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (callee.has_value())
                if (auto* v = std::get_if<Variable>(&*callee)) r.insert(*v);
            for (const auto& a : args)
                if (auto* v = std::get_if<Variable>(&a)) r.insert(*v);
            return r;
        }

        std::set<Variable> writes() const override {
            std::set<Variable> w;
            if (dst.has_value()) w.insert(*dst);
            return w;
        }
    };


    /* ============================================================
     * CallInstruction  —  call callee ( args? )
     * ============================================================ */
    class CallInstruction : public Instruction {
        std::optional<Callee> callee;
        std::vector<T>        args;
    public:
        CallInstruction() : Instruction(InstructionType::Call) {}

        void setCallee(Callee c) { callee = std::move(c); }
        void addArg(T t)         { args.push_back(std::move(t)); }

        const std::optional<Callee>& getCallee() const { return callee; }
        const std::vector<T>&        getArgs()   const { return args; }

        bool verify() const override { return callee.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\tcall " + calleeToString(*callee) +
                   "(" + argsToString(args) + ")\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (callee.has_value())
                if (auto* v = std::get_if<Variable>(&*callee)) r.insert(*v);
            for (const auto& a : args)
                if (auto* v = std::get_if<Variable>(&a)) r.insert(*v);
            return r;
        }
    };


    /* ============================================================
     * NewArrayInstruction  —  var <- new Array(args)
     * ============================================================ */
    class NewArrayInstruction : public Instruction {
        std::optional<Variable> dst;
        std::vector<T>          args;
    public:
        NewArrayInstruction() : Instruction(InstructionType::AssignFromNewArray) {}

        void setDst(Variable v) { dst = std::move(v); }
        void addArg(T t)        { args.push_back(std::move(t)); }

        const std::optional<Variable>& getDst()  const { return dst; }
        const std::vector<T>&          getArgs() const { return args; }

        bool verify() const override { return dst.has_value() && !args.empty(); }

        std::string to_string() const override {
            assert(verify());
            return "\t" + dst->to_string() + " <- new Array(" + argsToString(args) + ")\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            for (const auto& a : args)
                if (auto* v = std::get_if<Variable>(&a)) r.insert(*v);
            return r;
        }

        std::set<Variable> writes() const override {
            std::set<Variable> w;
            if (dst.has_value()) w.insert(*dst);
            return w;
        }
    };


    /* ============================================================
     * NewTupleInstruction  —  var <- new Tuple(t)
     * ============================================================ */
    class NewTupleInstruction : public Instruction {
        std::optional<Variable> dst;
        std::optional<T>        size;
    public:
        NewTupleInstruction() : Instruction(InstructionType::AssignFromNewTuple) {}

        void setDst(Variable v) { dst = std::move(v); }
        void setSize(T t)       { size = std::move(t); }

        const std::optional<Variable>& getDst()  const { return dst; }
        const std::optional<T>&        getSize() const { return size; }

        bool verify() const override { return dst.has_value() && size.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\t" + dst->to_string() + " <- new Tuple(" + tToString(*size) + ")\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (size.has_value())
                if (auto* v = std::get_if<Variable>(&*size)) r.insert(*v);
            return r;
        }

        std::set<Variable> writes() const override {
            std::set<Variable> w;
            if (dst.has_value()) w.insert(*dst);
            return w;
        }
    };


    /* ============================================================
     * BrInstruction  —  br label
     * ============================================================ */
    class BrInstruction : public Instruction {
        std::optional<Label> target;
    public:
        BrInstruction() : Instruction(InstructionType::Br) {}

        void setTarget(Label l) { target = std::move(l); }
        const std::optional<Label>& getTarget() const { return target; }

        bool verify() const override { return target.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\tbr " + target->to_string() + "\n";
        }
    };


    /* ============================================================
     * BrTInstruction  —  br t label label
     *   IR's conditional branch has two labels (true/false).
     * ============================================================ */
    class BrTInstruction : public Instruction {
        std::optional<T>     cond;
        std::optional<Label> trueTarget;
        std::optional<Label> falseTarget;
    public:
        BrTInstruction() : Instruction(InstructionType::BrT) {}

        void setCond(T t)            { cond = std::move(t); }
        void setTrueTarget(Label l)  { trueTarget  = std::move(l); }
        void setFalseTarget(Label l) { falseTarget = std::move(l); }

        const std::optional<T>&     getCond()        const { return cond; }
        const std::optional<Label>& getTrueTarget()  const { return trueTarget; }
        const std::optional<Label>& getFalseTarget() const { return falseTarget; }

        bool verify() const override {
            return cond.has_value() && trueTarget.has_value() && falseTarget.has_value();
        }

        std::string to_string() const override {
            assert(verify());
            return "\tbr " + tToString(*cond) + " " +
                   trueTarget->to_string() + " " + falseTarget->to_string() + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (cond.has_value())
                if (auto* v = std::get_if<Variable>(&*cond)) r.insert(*v);
            return r;
        }
    };


    /* ============================================================
     * ReturnInstruction  —  return
     * ============================================================ */
    class ReturnInstruction : public Instruction {
    public:
        ReturnInstruction() : Instruction(InstructionType::Return) {}
        bool verify() const override { return true; }
        std::string to_string() const override { return "\treturn\n"; }
    };


    /* ============================================================
     * ReturnTInstruction  —  return t
     * ============================================================ */
    class ReturnTInstruction : public Instruction {
        std::optional<T> value;
    public:
        ReturnTInstruction() : Instruction(InstructionType::ReturnT) {}

        void setValue(T t) { value = std::move(t); }
        const std::optional<T>& getValue() const { return value; }

        bool verify() const override { return value.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\treturn " + tToString(*value) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (value.has_value())
                if (auto* v = std::get_if<Variable>(&*value)) r.insert(*v);
            return r;
        }
    };


    /* ============================================================
     * LabelInstruction  —  :name (standalone, starts a basic block)
     * ============================================================ */
    class LabelInstruction : public Instruction {
        std::optional<Label> label;
    public:
        LabelInstruction() : Instruction(InstructionType::Label) {}

        void setLabel(Label l) { label = std::move(l); }
        const std::optional<Label>& getLabel() const { return label; }

        bool verify() const override { return label.has_value(); }

        std::string to_string() const override {
            assert(verify());
            return "\t" + label->to_string() + "\n";
        }
    };


    /* ============================================================
     * BasicBlock  —  bb ::= label i* te
     *   IR is block-structured: each block starts with a label and
     *   ends with a terminator 
     * ============================================================ */
    struct BasicBlock {
        std::unique_ptr<LabelInstruction> label;                  // the leading label
        std::vector<std::unique_ptr<Instruction>> instructions;   // body i*
        std::unique_ptr<Instruction> terminator;                  // te
    };

    

    /* ============================================================
     * Function  —  f ::= define T l (pars) { bb+ }
     * ============================================================ */
    class Function : public ASTNode {
        Type                       returnType;     // T ::= type | void
        FunctionName               name;
        std::vector<Type>          paramTypes;     // parallel to params
        std::vector<Variable>      params;
        const BasicBlock* entry = nullptr;
    public:
        // std::map<std::string, Type> varTypes; // variables types 

        std::vector<std::list<BasicBlock*>> traces; 
        std::vector<std::unique_ptr<BasicBlock>> blocks;  // list
        std::map<const BasicBlock*, std::vector<const BasicBlock*>> successors; //edges 
   

        Function() = default;

        const std::string&                getName()       const { return name.name; }
        const Type&                       getReturnType() const { return returnType; }
        const std::vector<Variable>&      getParams()     const { return params; }
        const std::vector<Type>&          getParamTypes() const { return paramTypes; }
        int getNumParams() const { return static_cast<int>(params.size()); }

        void setName(FunctionName n)      { name = std::move(n); }
        void setEntry(const BasicBlock* e) {
            if (entry) return;
            entry = e;
        }
        void setReturnType(Type t)        { returnType = std::move(t); }
        void addParam(Type t, Variable v) {
            paramTypes.push_back(std::move(t));
            params.push_back(std::move(v));
        }

     
     


        bool verify() const override {
            if (name.name.empty()) return false;
            return !blocks.empty();
        }

        bool dfs_find_loop(const BasicBlock* current,
                   const BasicBlock* target,
                   std::set<const BasicBlock*>& visited) {
            if (!visited.insert(current).second) return false; // already explored

            for (const BasicBlock* s : successors[current]) {
                if (s == target) return true;                       // direct back to start
                if (dfs_find_loop(s, target, visited)) return true; // transitively reaches start
            }
            return false;
        }
 

        BasicBlock* select_next(BasicBlock* cur, std::vector<const BasicBlock*> next){
            if (next.empty()) return nullptr;

            for (const BasicBlock* candidate : next) {
                std::set<const BasicBlock*> visited;
                if (dfs_find_loop(candidate, cur, visited)) {
                    return const_cast<BasicBlock*>(candidate);
                }
            }

            return const_cast<BasicBlock*>(next.front());
            // find it through a dominator tree. 

        }


        void build_traces() {
            std::set<BasicBlock*> marked;
            for (size_t counter = 0; counter < blocks.size(); ++counter) {
                BasicBlock* bb = blocks[counter].get();
                if (marked.count(bb)) continue;                   // already in some trace

                std::list<BasicBlock*> tr;
                while (bb && marked.find(bb) == marked.end()) {
                    marked.insert(bb);
                    tr.push_back(bb);
                    std::vector<const BasicBlock*> next = successors[bb];
                    bb = select_next(bb, next);                   
                }
                if (!tr.empty()) traces.push_back(std::move(tr));
            }
        }

        void build_successor_graph() {
            for (const auto& bb : blocks) {
                const auto& t = bb->terminator;
                assert(t->type == InstructionType::Return  ||
                    t->type == InstructionType::ReturnT ||
                    t->type == InstructionType::Br      ||
                    t->type == InstructionType::BrT);

                if (t->type == InstructionType::Return ||
                    t->type == InstructionType::ReturnT) {
                    successors[bb.get()] = {};
                }

                if (auto* branch = dynamic_cast<BrInstruction*>(t.get())) {
                    for (const auto& bt : blocks) {
                        const auto& l = bt->label;
                        if (branch->getTarget() == l->getLabel()) {
                            successors[bb.get()] = { bt.get() };
                            break;
                        }
                    }
                }

                if (auto* branch = dynamic_cast<BrTInstruction*>(t.get())) {
                    successors[bb.get()] = {};
                    for (const auto& bt : blocks) {
                        const auto& l = bt->label;
                        if (branch->getTrueTarget()  == l->getLabel() ||
                            branch->getFalseTarget() == l->getLabel()) {
                            successors[bb.get()].push_back(bt.get());
                        }
                    }
                }
            }
        }

        std::string to_string() const override {
            std::string out = "define " + returnType.to_string() + " " +
                              name.to_string() + "(";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i) out += ", ";
                out += paramTypes[i].to_string() + " " + params[i].to_string();
            }
            out += ") {\n";
            if (!traces.empty()){
                assert(traces[0].front() == entry);
                for (const auto& tr : traces) {
                    if (tr.empty()) continue;   
                                          // skip empty traces
                    for (const BasicBlock* bb : tr) {
                        if (bb->label)      out += bb->label->to_string();
                        for (const auto& i : bb->instructions) out += i->to_string();
                        if (bb->terminator) out += bb->terminator->to_string();
                    }
                }
            }else {
                for (const auto& bb : blocks) {
                    if (bb->label)      out += bb->label->to_string();
                    for (const auto& i : bb->instructions) out += i->to_string();
                    if (bb->terminator) out += bb->terminator->to_string();
                }
            }
            
            out += "}\n";

            // out += "size :" + std::to_string(blocks.size()) + '\n';
            return out;
        }
    };


    /* ============================================================
     * Program  —  p ::= f+
     * ============================================================ */
    class Program : public ASTNode {
    public:
        std::vector<Function> functions;

        Program() = default;

        std::string to_string() const override {
            
            std::string out;

            
            for (auto& f : functions) {
                out += f.to_string();
                out += "\n";
            }
            return out;
        }

        bool verify() const override {
            for (auto& f : functions)
                if (f.getName() == "main") return true;
            throw std::runtime_error("@main function doesn't exist");
        }
    };

}