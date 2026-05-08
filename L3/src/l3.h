
#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <cassert>
#include <optional>
#include <type_traits>
#include <tree.h>
#include <ast_leaves.h>
#include <set>


namespace L3 {

    // forward declaration. 
    class Function;
    class Context;
    struct LivenessInfo;
    std::vector<LivenessInfo> compute_liveness(const Function& f);
    std::vector<LivenessInfo> compute_liveness(const Context& ctx);


   
    /* Variants  */
    // t ::= var | N
    using T = std::variant<Variable, Number>;

    // u ::= var | l
    //   l is a function name like "@foo" — stored as std::string per your convention
    using U = std::variant<Variable, FunctionName>;

    // s ::= t | label | l
    //   = var | N | :label | @function
    using S = std::variant<Variable, Number, Label, FunctionName>;

    // callee ::= u | builtin
    using Callee = std::variant<Variable, FunctionName, BuiltinCallee>;

    // for liveness analysis 
    struct LivenessInfo {
        std::set<Variable> in;
        std::set<Variable> out;
    };



    

    // Instruction Herarchy 
    class Instruction : public ASTNode {
        public:
            InstructionType type;
            Instruction() = delete;
            Instruction(InstructionType t) : type(t) {}
            virtual ~Instruction() = default;

            bool verify() const override { return true; }
            virtual std::string to_string() const = 0;
            virtual std::unique_ptr<TreeNode> to_tree() const { return nullptr; }
            virtual std::set<Variable> reads() const {return {};}
            virtual std::set<Variable> writes() const {return {};}


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

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !src.has_value()) return nullptr;
        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst),  
                std::make_unique<TreeNode>(*src)
            });
    }

    std::string to_string() const override {
        assert(verify());
        std::string rhs = std::visit([](const auto& x) -> std::string {
            using V = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<V, std::string>) return x;
            else                                          return x.to_string();
        }, *src);
        return "\t" + dst->to_string() + " <- " + rhs + "\n";
    }


    std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (!src.has_value()) return r;
            if (auto* var = std::get_if<Variable>(&*src)){
                r.insert(*var);
            }
            return r;
    }

    std::set<Variable> writes() const override {
            std::set<Variable> r;
            if (!dst.has_value()) return r;
            r.insert(dst.value());
            return r;
    }


};


/* ============================================================
 * OpInstruction  —  var <- t op t
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

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !op.has_value() || !lhs.has_value() || !rhs.has_value()) return nullptr;
        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst), 
                std::make_unique<TreeNode>(BinOpNode{
                        op.value(),
                        std::make_unique<TreeNode>(*lhs),  
                        std::make_unique<TreeNode>(*rhs)
                    })
            });
        }
            
    

    static std::string opToString(Op o) {
        switch (o) {
            case Op::Add: return "+";
            case Op::Sub: return "-";
            case Op::Mul: return "*";
            case Op::And: return "&";
            case Op::Shl: return "<<";
            case Op::Shr: return ">>";
        }
        throw std::runtime_error("OpInstruction: unknown Op");
    }

    std::string to_string() const override {
        assert(verify());
        auto tStr = [](const T& v) {
            return std::visit([](const auto& x) { return x.to_string(); }, v);
        };
        return "\t" + dst->to_string() + " <- " +
               tStr(*lhs) + " " + opToString(*op) + " " + tStr(*rhs) + "\n";
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (lhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*lhs)) {
                r.insert(*var);
            }
        }
        if (rhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*rhs)) {
                r.insert(*var);
            }
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }
};


/* ============================================================
 * CmpInstruction  —  var <- t cmp t
 * ============================================================ */
class CmpInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<T>        lhs;
    std::optional<Cmp>      cmp;
    std::optional<T>        rhs;
public:
    CmpInstruction() : Instruction(InstructionType::AssignFromCmp) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setLhs(T t)        { lhs = std::move(t); }
    void setCmp(Cmp c)      { cmp = c; }
    void setRhs(T t)        { rhs = std::move(t); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<T>&        getLhs() const { return lhs; }
    const std::optional<Cmp>&      getCmp() const { return cmp; }
    const std::optional<T>&        getRhs() const { return rhs; }

    bool verify() const override {
        return dst.has_value() && lhs.has_value() && cmp.has_value() && rhs.has_value();
    }

    static std::string cmpToString(Cmp c) {
        switch (c) {
            case Cmp::Lt: return "<";
            case Cmp::Le: return "<=";
            case Cmp::Eq: return "=";
            case Cmp::Ge: return ">=";
            case Cmp::Gt: return ">";
        }
        throw std::runtime_error("CmpInstruction: unknown Cmp");
    }

    std::string to_string() const override {
        assert(verify());
        auto tStr = [](const T& v) {
            return std::visit([](const auto& x) { return x.to_string(); }, v);
        };
        return "\t" + dst->to_string() + " <- " +
               tStr(*lhs) + " " + cmpToString(*cmp) + " " + tStr(*rhs) + "\n";
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !cmp.has_value() || !lhs.has_value() || !rhs.has_value()) return nullptr;
        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst), 
                std::make_unique<TreeNode>(CompareNode{
                        cmp.value(),
                        std::make_unique<TreeNode>(*lhs),  
                        std::make_unique<TreeNode>(*rhs)
                    })
            });
        }
    
    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (lhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*lhs)) {
                r.insert(*var);
            }
        }
        if (rhs.has_value()) {
            if (auto* var = std::get_if<Variable>(&*rhs)) {
                r.insert(*var);
            }
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }
                
};


/* ============================================================
 * LoadInstruction  —  var <- load var
 * ============================================================ */
class LoadInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<Variable> src;
public:
    LoadInstruction() : Instruction(InstructionType::AssignFromLoad) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setSrc(Variable v) { src = std::move(v); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<Variable>& getSrc() const { return src; }

    bool verify() const override { return dst.has_value() && src.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\t" + dst->to_string() + " <- load " + src->to_string() + "\n";
    }

    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !src.has_value()) return nullptr;

        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(*dst),
                std::make_unique<TreeNode>(
                    LoadNode{
                        std::make_unique<TreeNode>(*src)
                    }
                )
            }
            );
        }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (src.has_value()) {
            r.insert(*src);
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }



};


/* ============================================================
 * StoreInstruction  —  store var <- s
 * ============================================================ */
class StoreInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<S>        src;
public:
    StoreInstruction() : Instruction(InstructionType::Store) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setSrc(S s)        { src = std::move(s); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<S>&        getSrc() const { return src; }

    bool verify() const override { return dst.has_value() && src.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string rhs = std::visit([](const auto& x) -> std::string {
            using V = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<V, std::string>) return x;
            else                                          return x.to_string();
        }, *src);
        return "\tstore " + dst->to_string() + " <- " + rhs + "\n";
    }


    std::unique_ptr<TreeNode> to_tree() const override {
        if (!dst.has_value() || !src.has_value()) return nullptr;

        return std::make_unique<TreeNode>(
            AssignNode{
                std::make_unique<TreeNode>(StoreNode{std::make_unique<TreeNode>(*dst)}),
                std::make_unique<TreeNode>(*src)
            }
            );
        }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (dst.has_value()) {
            r.insert(*dst);  // address being stored into — that's a READ of the address variable
        }
        if (src.has_value()) {
            if (auto* var = std::get_if<Variable>(&*src)) {
                r.insert(*var);  // value being stored, if it's a variable
            }
        }
        return r;
    }
// store writes to memory, not to a variable

};


/* ============================================================
 * Helpers shared by call instructions
 * ============================================================ */
inline std::string calleeToString(const Callee& c) {
    return std::visit([](const auto& x) -> std::string {
        using V = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<V, BuiltinCallee>) return builtinCalleeToString(x);
        else if constexpr (std::is_same_v<V, FunctionName>) return x.name;  // includes '@'
        else                                                return x.to_string();
    }, c);
}

inline std::string argsToString(const std::vector<T>& args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) out += ", ";
        out += std::visit([](const auto& x) { return x.to_string(); }, args[i]);
    }
    return out;
}


/* ============================================================
 * VarCallInstruction  —  var <- call callee ( args )
 * ============================================================ */
class VarCallInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<Callee>   callee;
    std::vector<T>          args;
public:
    VarCallInstruction() : Instruction(InstructionType::AssignFromCall) {}

    void setDst(Variable v)    { dst = std::move(v); }
    void setCallee(Callee c)   { callee = std::move(c); }
    void addArg(T t)           { args.push_back(std::move(t)); }

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
        if (callee.has_value()) {
            if (auto* var = std::get_if<Variable>(&*callee)) {
                r.insert(*var);  // callee is a function pointer in a variable
            }
        }
        for (const auto& arg : args) {
            if (auto* var = std::get_if<Variable>(&arg)) {
                r.insert(*var);  // each Variable arg is a read
            }
        }
        return r;
    }

    std::set<Variable> writes() const override {
        std::set<Variable> w;
        if (dst.has_value()) {
            w.insert(*dst);
        }
        return w;
    }

};


/* ============================================================
 * CallInstruction  —  call callee ( args )
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
        if (callee.has_value()) {
            if (auto* var = std::get_if<Variable>(&*callee)) {
                r.insert(*var);  // callee is a function pointer in a variable
            }
        }
        for (const auto& arg : args) {
            if (auto* var = std::get_if<Variable>(&arg)) {
                r.insert(*var);  // each Variable arg is a read
            }
        }
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

    std::string to_string() const override {
        return "\treturn\n";
    }
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
        std::string v = std::visit([](const auto& x) { return x.to_string(); }, *value);
        return "\treturn " + v + "\n";
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (value.has_value()) {
            if (auto* var = std::get_if<Variable>(&*value)) {
                r.insert(*var);
            }
        }
        return r;
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
 * BrTInstruction  —  br t label
 * ============================================================ */
class BrTInstruction : public Instruction {
    std::optional<T>     cond;
    std::optional<Label> target;
public:
    BrTInstruction() : Instruction(InstructionType::BrT) {}

    void setCond(T t)       { cond = std::move(t); }
    void setTarget(Label l) { target = std::move(l); }

    const std::optional<T>&     getCond()   const { return cond; }
    const std::optional<Label>& getTarget() const { return target; }

    bool verify() const override { return cond.has_value() && target.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string c = std::visit([](const auto& x) { return x.to_string(); }, *cond);
        return "\tbr " + c + " " + target->to_string() + "\n";
    }

    std::set<Variable> reads() const override {
        std::set<Variable> r;
        if (cond.has_value()) {
            if (auto* var = std::get_if<Variable>(&*cond)) {
                r.insert(*var);
            }
        }
        return r;
    }
};


/* ============================================================
 * LabelInstruction  —  :name (standalone)
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
        return "\t" +  label->to_string() + "\n";   // no leading tab — labels are flush-left
    }
};







    class Context {
        public:
            std::vector<std::unique_ptr<Instruction>> instructions;
            std::vector<std::unique_ptr<TreeNode>> trees;
            std::vector<LivenessInfo> liveAnalysisReport;
            
            Context() = default;
            // Add instructions one at a time.
            void add(std::unique_ptr<Instruction> instr) {
                instructions.push_back(std::move(instr));
                // liveAnalysisReport.insert(lar.begin(), lar.end());

            }

            // overload — assign liveness slice to this context
            void add(std::vector<LivenessInfo>::iterator begin, 
                    std::vector<LivenessInfo>::iterator end) {
                liveAnalysisReport.assign(begin, end);
            }


            // Read-only access.
            const std::vector<std::unique_ptr<Instruction>>& get() const {
                return instructions;
            }

            bool empty() const { return instructions.empty(); }
            size_t size() const { return instructions.size(); }

            bool is_terminated() const {
                if (instructions.empty()) return false;
                InstructionType t = instructions.back()->type;
                return t == InstructionType::Br
                    || t == InstructionType::BrT
                    || t == InstructionType::Return
                    || t == InstructionType::ReturnT;
            }
            void print_trees(bool debug = false) const {
                if (!debug) return;

                std::cerr << "=== Context Trees ===\n";
                std::cerr << "instructions: " << instructions.size() 
                        << " trees: " << trees.size() 
                        << " liveness: " << liveAnalysisReport.size() << "\n\n";

                for (int i = 0; i < (int)instructions.size(); i++) {
                    std::cerr << "[" << i << "] " << instructions[i]->to_string();
                    
                    if (i < (int)trees.size()) {
                        if (trees[i]) {
                            std::cerr << "     tree: " << tree_to_string(*trees[i]) << "\n";
                        } else {
                            std::cerr << "     tree: nullptr\n";
                        }
                    } else {
                        std::cerr << "     tree: (no tree entry)\n";
                    }

                    if (i < (int)liveAnalysisReport.size()) {
                        std::cerr << "     in : { ";
                        for (const auto& v : liveAnalysisReport[i].in)  std::cerr << v.to_string() << " ";
                        std::cerr << "}\n";
                        std::cerr << "     out: { ";
                        for (const auto& v : liveAnalysisReport[i].out) std::cerr << v.to_string() << " ";
                        std::cerr << "}\n";
                    }
                    std::cerr << "\n";
                }
            }

            void build_tree(){
                for (auto& instr : instructions) {
                     trees.push_back(instr->to_tree()); 
                }
            }

            void merge_tree(){
                // find the context when to merge them and just call the merging function. 
                if (instructions.size() != trees.size()) {
                    std::cerr << "ASSERT FAIL: instructions.size()=" << instructions.size()
                            << " trees.size()=" << trees.size() << "\n";
                    for (int i = 0; i < (int)instructions.size(); i++) {
                        std::cerr << "  instr[" << i << "] = " << instructions[i]->to_string();
                    }
                    assert(false);
                }

                if (instructions.size() != liveAnalysisReport.size()) {
                    std::cerr << "ASSERT FAIL: instructions.size()=" << instructions.size()
                            << " liveAnalysisReport.size()=" << liveAnalysisReport.size() << "\n";
                    assert(false);
                }

                bool changed = true;
                while (changed)

                for (int i = 0; i < (int)instructions.size(); i++) {
                    print_trees(true);
                    if (!trees[i]) continue;


                    const auto& live = liveAnalysisReport[i];
                    const auto  type = instructions[i]->type;

                    bool is_pure = (type == InstructionType::AssignFromS  ||
                                    type == InstructionType::AssignFromOp ||
                                    type == InstructionType::AssignFromCmp ||
                                    type == InstructionType::AssignFromLoad);
                        // in = {}, out = {} --- it's a definition never used again. 
                    if (is_pure && live.in.empty() && live.out.empty()) {
                        instructions.erase(instructions.begin() + i);
                        trees.erase(trees.begin() + i);
                        liveAnalysisReport.erase(liveAnalysisReport.begin() + i);
                        continue;
                    }


                    // one sided difference checker :  // v is in out[i] but not in in[i]
                    // meaning %v is DEFINED at instruction i
                    std::set<Variable> diff;
                    std::set_difference(liveAnalysisReport[i].out.begin(), liveAnalysisReport[i].out.end(),
                                        liveAnalysisReport[i].in.begin(),  liveAnalysisReport[i].in.end(),
                                        std::inserter(diff, diff.begin()));

                    if (diff.empty()) continue;  // no variable defined here
                    assert(diff.size() == 1); // each instruction defines at most one variable 
                    const Variable& defined = *diff.begin();



                    auto find_death = [&](const Variable& var, int start) -> int {

                        for (int k = start; k < (int)liveAnalysisReport.size(); k++){
                            // in "in" but not in "out" -> dies here
                            if (liveAnalysisReport[k].in.count(var) && !liveAnalysisReport[k].out.count(var)){
                                return k;
                            }
                        }
                        return (int)liveAnalysisReport.size(); // escapes context 
                    };
                    
                    // important to safe guard from var being over context 
                    int death = find_death(defined, i + 1); // might not work due to the shrinking of liveAnalysisReport size 

                    if (death >= (int)instructions.size()) continue; // escapes don't merge 

                    int use_count = 0;
                    bool all_merged = true;

                    for (int j = i + 1; j <= death; j++) {
                        bool is_redef = instructions[j]->writes().count(defined);

                        if (instructions[j]->reads().count(defined)) {
                            use_count++;
                            auto source_copy = clone_tree(*trees[i]);
                            auto merged = L3::merge_tree(std::move(source_copy), std::move(trees[j]));
                            if (merged) {
                                trees[j] = std::move(merged);
                            } else {
                                all_merged = false;
                            }
                        }

                        if (is_redef) break;  // it's redefintion 
                    }
                                                            

                    // only erase instruction i if every use was successfully merged
                    if (use_count > 0 && all_merged) {
                        instructions.erase(instructions.begin() + i);
                        trees.erase(trees.begin() + i);
                        liveAnalysisReport.erase(liveAnalysisReport.begin() + i);
                        i--;
                    }

                
            }
        }

    };




    class Function : public ASTNode {
            FunctionName name;
            std::vector<L3::Variable> params;
            
        public:
            
            std::vector<std::unique_ptr<Instruction>> instructions;
            std::vector<L3::Context> contexts;
            bool _is_context = false;

            Function() = default;

            std::string to_string() const override {
                std::string result;
                result += "define " + this->name.name + "(";
                for (size_t i = 0; i < params.size(); ++i) {
                    if (i > 0) result += ", ";
                    result += "%" + params[i].name;
                }
                result += ") {\n";
                // std::cerr << "joined" ;
                if (_is_context) {

                    
                    for (const auto& ctx : contexts) {
                        for (const auto& instr : ctx.get()) {
                            result += instr->to_string();
                        }
                    }
                } else {
                    for (const auto& instr : instructions) {
                        result += instr->to_string();
                    }
                }
                result += "}\n";
                return result;
            }

            const std::string& getName() const { return this -> name.name; }
            const std::vector<Variable>& getParams() const { return params; }
            int getNumParams() const { return static_cast<int>(params.size()); }

            void setName(FunctionName n) { this -> name = std::move(n); }
            void addParam(Variable v) { params.push_back(std::move(v)); }
            void setParams(std::vector<Variable> ps) { params = std::move(ps); }

            bool verify() const override {
                if (this->name.name.empty()) return false;
                return _is_context ? !contexts.empty() : !instructions.empty();
            }

            void build_blocks() {
                std::vector<LivenessInfo> result = compute_liveness(*this); 
                auto begin = result.begin();
                auto upto  = result.begin();

                Context current;
                for (auto& instr : instructions) {
                    if ((instr->type == InstructionType::Label || 
                        instr->type == InstructionType::Call) && !current.empty()) {
                        current.add(begin, upto);  // assign liveness before push
                        contexts.push_back(std::move(current));
                        current = Context{};
                        begin = upto;             // ← advance begin to start of new context
                    }

                    current.add(std::move(instr));
                    upto++;

                    if (current.is_terminated()) {
                        current.add(begin, upto); // assign liveness before push
                        contexts.push_back(std::move(current));
                        current = Context{};
                        begin = upto;             // ← advance begin
                    }
                }

                if (!current.empty()) {
                    current.add(begin, upto);     // don't forget the last context
                    contexts.push_back(std::move(current));
                }

                instructions.clear();
                this->_is_context = true;
            }



        };


    class Program : public ASTNode {
        public:
        std::vector<Function> functions;

        Program() = default;

        std::string to_string() const override {
            std::string result;
            for (auto& function : functions) {
                result += function.to_string();
                result += "\n";
            }
            return result;
        }

        bool verify() const override {
            for (auto& function : functions) {
                if (function.getName() == "main") return true;
            }
            throw std::runtime_error("@main function doesn't exist");
            return false;
        }
    };

  
}

