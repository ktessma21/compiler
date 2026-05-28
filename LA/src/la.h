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
#include <ast_leaves.h>
#include <set>



namespace LA {

    // forward declaration. 
    class Function;

            // Instruction Herarchy 
    class Instruction : public ASTNode {
        public:
            InstructionType type;
            int64_t lineNumber = 0;
            Instruction() = delete;
            Instruction(InstructionType t, int64_t line) : type(t), lineNumber(line) {}
            virtual ~Instruction() = default;

            void setLineNumber(int64_t line) { lineNumber = line; }
            int64_t getLineNumber() const { return lineNumber; }
            bool verify() const override { return true; }
            virtual std::string to_string() const = 0;
            virtual std::vector<T> toDecode() const { return {}; }
            virtual std::vector<Variable> toEncode() const { return {}; }
            


    };
    
    




    /* ============================================================
 * DeclInstruction  —  type name
 * ============================================================ */
class DeclInstruction : public Instruction {
    std::optional<Type>     varType;
    std::optional<Variable> var;
public:
    DeclInstruction() : Instruction(InstructionType::Decl, 0) {}

    void setType(Type t)    { varType = std::move(t); }
    void setVar(Variable v) { var = std::move(v); }

    const std::optional<Type>&     getType() const { return varType; }
    const std::optional<Variable>& getVar()  const { return var; }

    bool verify() const override { return varType.has_value() && var.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\t" + varType->to_string() + " " + var->to_string() + "\n";
    }
};


/* ============================================================
 * AssignInstruction  —  name <- t
 * ============================================================ */
class AssignInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<T>        src;
public:
    AssignInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::AssignFromT, lineNumber) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setSrc(T t)        { src = std::move(t); }

    const std::optional<Variable>& getDst() const { return dst; }
    const std::optional<T>&        getSrc() const { return src; }

    bool verify() const override { return dst.has_value() && src.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string rhs = std::visit([](const auto& x) { return x.to_string(); }, *src);
        return "\t" + dst->to_string() + " <- " + rhs + "\n";
    }
};


/* ============================================================
 * OpInstruction  —  name <- t op t
 *   (LA folds comparisons into op, so there is no separate Cmp.)
 * ============================================================ */
class OpInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<T>        lhs;
    std::optional<Op>       op;
    std::optional<T>        rhs;
public:

    bool just_decoded = false;
    OpInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::AssignFromOp, lineNumber) {}

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
        auto tStr = [](const T& v) {
            return std::visit([](const auto& x) { return x.to_string(); }, v);
        };
        return "\t" + dst->to_string() + " <- " +
               tStr(*lhs) + " " + opToString(*op) + " " + tStr(*rhs) + "\n";
    }

    std::vector<Variable> toEncode() const override {
        std::vector<Variable> vars;
        vars.push_back(*dst);
        return vars;
    }

    std::vector<T> toDecode() const override {
        std::vector<T> vars;
        vars.push_back(*lhs);
        vars.push_back(*rhs);
        return vars;
    }
};


/* ============================================================
 * ArrayLoadInstruction  —  name <- name([t])+
 * ============================================================ */
class ArrayLoadInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<Variable> src;
    std::vector<T>          indices;
public:
    ArrayLoadInstruction(int64_t lineNumber) : Instruction(InstructionType::ArrayLoad, lineNumber) {}

    void setDst(Variable v)  { dst = std::move(v); }
    void setSrc(Variable v)  { src = std::move(v); }
    void addIndex(T t)       { indices.push_back(std::move(t)); }

    const std::optional<Variable>& getDst()     const { return dst; }
    const std::optional<Variable>& getSrc()     const { return src; }
    const std::vector<T>&          getIndices() const { return indices; }

    bool verify() const override {
        return dst.has_value() && src.has_value() && !indices.empty();
    }

    std::string to_string() const override {
        assert(verify());
        std::string subs;
        for (const auto& ix : indices)
            subs += "[" + std::visit([](const auto& x) { return x.to_string(); }, ix) + "]";
        return "\t" + dst->to_string() + " <- " + src->to_string() + subs + "\n";
    }

    std::vector<T> toDecode() const override {
         std::vector<T> vars;
        for (const auto& ix : indices)
            vars.push_back(ix);
        return vars;
    }
};


/* ============================================================
 * ArrayStoreInstruction  —  name([t])+ <- t
 * ============================================================ */
class ArrayStoreInstruction : public Instruction {
    std::optional<Variable> dst;
    std::vector<T>          indices;
    std::optional<T>        src;
    std::optional<Callee>   src_c;
public:
    ArrayStoreInstruction(int64_t lineNumber) : Instruction(InstructionType::ArrayStore, lineNumber) {}
    void setDst(Variable v) { dst = std::move(v); }
    void addIndex(T t)      { indices.push_back(std::move(t)); }
    void setSrc(T t)        { src = std::move(t); }
    void setSrcCallee(Callee c) { src_c = std::move(c); }

    const std::optional<Callee>& getSrcCallee() const { return src_c; }
    const std::optional<Variable>& getDst()     const { return dst; }
    const std::vector<T>&          getIndices() const { return indices; }
    const std::optional<T>&        getSrc()     const { return src; }

    bool verify() const override {
        return dst.has_value() && !indices.empty() && (src.has_value() || src_c.has_value());
    }

    std::string to_string() const override {
        assert(verify());
        std::string subs;
        for (const auto& ix : indices)
            subs += "[" + std::visit([](const auto& x) { return x.to_string(); }, ix) + "]";
        std::string rhs = std::visit([](const auto& x) { return x.to_string(); }, *src);
        return "\t" + dst->to_string() + subs + " <- " + rhs + "\n";
    }


    std::vector<T> toDecode() const override {
        std::vector<T> vars;
        for (const auto& ix : indices)
            vars.push_back(ix);
        return vars;
    }

};


/* ============================================================
 * LengthInstruction  —  name <- length name t?
 * ============================================================ */
class LengthInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<Variable> array;
    std::optional<T>        dim;     // optional dimension
public:
    LengthInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::Length, lineNumber) {}

    void setDst(Variable v)   { dst = std::move(v); }
    void setArray(Variable v) { array = std::move(v); }
    void setDim(T t)          { dim = std::move(t); }

    const std::optional<Variable>& getDst()   const { return dst; }
    const std::optional<Variable>& getArray() const { return array; }
    const std::optional<T>&        getDim()   const { return dim; }

    bool verify() const override { return dst.has_value() && array.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string out = "\t" + dst->to_string() + " <- length " + array->to_string();
        if (dim.has_value())
            out += " " + std::visit([](const auto& x) { return x.to_string(); }, *dim);
        return out + "\n";
    }

    std::vector<T> toDecode() const override {
        std::vector<T> vars;
        if (dim.has_value()) vars.push_back(*dim);
        return vars;
    }
};


/* ============================================================
 * Helper shared by call / new-Array instructions
 * ============================================================ */
inline std::string argsToString(const std::vector<T>& args) {
    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) out += ", ";
        out += std::visit([](const auto& x) { return x.to_string(); }, args[i]);
    }
    return out;
}


/* ============================================================
 * NewArrayInstruction  —  name <- new Array( args )
 * ============================================================ */
class NewArrayInstruction : public Instruction {
    std::optional<Variable> dst;
    std::vector<T>          args;
public:
    NewArrayInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::NewArray, lineNumber) {}

    void setDst(Variable v) { dst = std::move(v); }
    void addArg(T t)        { args.push_back(std::move(t)); }

    const std::optional<Variable>& getDst()  const { return dst; }
    const std::vector<T>&          getArgs() const { return args; }
    std::vector<T>&          getArgs()  { return args; }

    bool verify() const override { return dst.has_value(); }

    std::string to_string() const override {
        assert(verify());
        return "\t" + dst->to_string() + " <- new Array(" + argsToString(args) + ")\n";
    }
};


/* ============================================================
 * NewTupleInstruction  —  name <- new Tuple( t )
 * ============================================================ */
class NewTupleInstruction : public Instruction {
    std::optional<Variable> dst;
    std::optional<T>        size;
public:
    NewTupleInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::NewTuple, lineNumber) {}

    void setDst(Variable v) { dst = std::move(v); }
    void setSize(T t)       { size = std::move(t); }

    const std::optional<Variable>& getDst()  const { return dst; }
    const std::optional<T>&        getSize() const { return size; }

    bool verify() const override { return dst.has_value() && size.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string sz = std::visit([](const auto& x) { return x.to_string(); }, *size);
        return "\t" + dst->to_string() + " <- new Tuple(" + sz + ")\n";
    }
};


/* ============================================================
 * VarCallInstruction  —  name <- name ( args )
 * ============================================================ */
class VarCallInstruction : public Instruction {
    std::optional<Variable>     dst;
    std::optional<Callee> callee;
    std::vector<T>              args;
public:
    VarCallInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::AssignFromCall, lineNumber) {}

    void setDst(Variable v)        { dst = std::move(v); }
    void setCallee(Callee c) { callee = std::move(c); }
    void addArg(T t)               { args.push_back(std::move(t)); }

    const std::optional<Variable>&     getDst()    const { return dst; }
    const std::optional<Callee>& getCallee() const { return callee; }
    const std::vector<T>&              getArgs()   const { return args; }
    std::vector<T>& getArgs() { return args; }

    bool verify() const override { return dst.has_value() && callee.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string c = std::visit([](const auto& x) { return x.to_string(); }, *callee);
        return "\t" + dst->to_string() + " <- " +
               c + "(" + argsToString(args) + ")\n";
    }


};


/* ============================================================
 * CallInstruction  —  name ( args )
 * ============================================================ */
class CallInstruction : public Instruction {
    std::optional<Callee> callee;
    std::vector<T>              args;
public:
    CallInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::Call, lineNumber) {}

    void setCallee(Callee c) { callee = std::move(c); }
    void addArg(T t)               { args.push_back(std::move(t)); }

    const std::optional<Callee>& getCallee() const { return callee; }
    const std::vector<T>&     getArgs() const  { return args; }
    std::vector<T>& getArgs() { return args; }


    bool verify() const override { return callee.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string c = std::visit([](const auto& x) { return x.to_string(); }, *callee);
        return "\t" + c + "(" + argsToString(args) + ")\n";
    }
};


/* ============================================================
 * ReturnInstruction  —  return
 * ============================================================ */
class ReturnInstruction : public Instruction {
public:
    ReturnInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::Return, lineNumber) {}

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
    ReturnTInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::ReturnT, lineNumber) {}

    void setValue(T t) { value = std::move(t); }
    const std::optional<T>& getValue() const { return value; }

    bool verify() const override { return value.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string v = std::visit([](const auto& x) { return x.to_string(); }, *value);
        return "\treturn " + v + "\n";
    }
};


/* ============================================================
 * BrInstruction  —  br label
 * ============================================================ */
class BrInstruction : public Instruction {
    std::optional<Label> target;
public:
    BrInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::Br, lineNumber) {}

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
    std::optional<Label> trueTarget;
    std::optional<Label> falseTarget;

public:
    BrTInstruction(int64_t lineNumber = 0) : Instruction(InstructionType::BrT, lineNumber){}

    void setCond(T t)       { cond = std::move(t); }
    void setTrueTarget(Label l) { trueTarget = std::move(l); }
    void setFalseTarget(Label l) { falseTarget = std::move(l); }

    const std::optional<T>&     getCond()   const { return cond; }
    const std::optional<Label>& getTrueTarget() const { return trueTarget; }
    const std::optional<Label>& getFalseTarget() const { return falseTarget; }

    bool verify() const override { return cond.has_value() && trueTarget.has_value() && falseTarget.has_value(); }

    std::string to_string() const override {
        assert(verify());
        std::string c = std::visit([](const auto& x) { return x.to_string(); }, *cond);
        return "\tbr " + c + " " + trueTarget->to_string() + " " + falseTarget->to_string() + "\n";
    }

    std::vector<T> toDecode() const override {
        std::vector<T> vars;
        if (cond.has_value()) vars.push_back(*cond);
        return vars;
    }
};


/* ============================================================
 * LabelInstruction  —  :name (standalone)
 * ============================================================ */
class LabelInstruction : public Instruction {
    std::optional<Label> label;
public:
    LabelInstruction() : Instruction(InstructionType::Label, 0) {}
 
    void setLabel(Label l) { label = std::move(l); }
    const std::optional<Label>& getLabel() const { return label; }
 
    bool verify() const override { return label.has_value(); }
 
    std::string to_string() const override {
        assert(verify());
        // Emit exactly one leading ':' regardless of whether the stored
        // Label::to_string() already carries one (prevents "::name").
        std::string s = label->to_string();
        if (s.empty() || s.front() != ':')
            s = ":" + s;
        return "\t" + s + "\n";
    }
};

/* ============================================================
 * RawInstruction  —  has only string (used in code generation).
 * ============================================================ */

class RawInstruction : public Instruction {
    std::string text;
public:
    explicit RawInstruction(std::string s, int64_t lineNumber = 0)
        : Instruction(InstructionType::Raw, lineNumber), text(std::move(s)) {}
 
    const std::string& getText() const { return text; }
    void setText(std::string s) { text = std::move(s); }
 
    bool verify() const override { return true; }
 
    // to_string() doubles as the emitted form: verbatim, no extra tabs.
    std::string to_string() const override { return text; }
};






    


    class Function : public ASTNode {
            Type returnType;
            FunctionName name;
            std::vector<Type>             paramTypes;
            std::vector<LA::Variable>     params;
            
        public:
            
            std::vector<std::unique_ptr<Instruction>> instructions;
            std::map<std::string, VarType> declTypes;

            Function() = default;

            const std::string& getName() const { return this -> name.name; }
            const Type& getReturnType() const { return returnType; }
            const std::vector<Variable>& getParams() const { return params; }
            const std::vector<Type>& getParamTypes() const { return paramTypes; }
            int getNumParams() const { return static_cast<int>(params.size()); }

            void setReturnType(Type t) { this -> returnType = std::move(t); }
            void setName(FunctionName n) { this -> name = std::move(n); }
            void addParam(Type t, Variable v) {
                paramTypes.push_back(std::move(t));
                params.push_back(std::move(v));
            }

            bool verify() const override {
                if (this->name.name.empty()) return false;
                return true;   // LA allows an empty body: { i* }
            }

            std::string to_string() const {
                std::string out = returnType.to_string() + " " + name.to_string() + "(";
                for (size_t i = 0; i < params.size(); ++i) {
                    if (i) out += ", ";
                    out += paramTypes[i].to_string() + " " + params[i].to_string();
                }
                out += ") {\n";
                for (const auto& ins : instructions) out += ins->to_string();
                out += "}\n";
                return out;
            }
        };


    class Program : public ASTNode {
        public:
        std::vector<Function> functions;
        std::map<std::string, VarType> declTypes;

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
            throw std::runtime_error("main function doesn't exist");
            return false;
        }
    };


    
  
}
