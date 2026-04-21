#pragma once

#include <string>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <cassert>
#include <optional>
#include <set>

namespace L2 {

    // ---------- Forward declarations of core types ----------

    class ASTNode {
    public:
        virtual ~ASTNode() = default;
        virtual std::string to_string() const = 0;
        virtual bool verify() const { return true; }
    };

    // Label and Variable both wrap a std::string, but must be distinct
    // types so they can appear as separate alternatives in std::variant.
    struct Label {
        std::string name;
        Label() = default;
        explicit Label(std::string s) : name(std::move(s)) {}
        bool operator==(const Label&) const = default;
        bool empty() {return name == "";}
    };

    struct Variable {
        std::string name;
        Variable() = default;
        explicit Variable(std::string s) : name(std::move(s)) {}
        bool operator==(const Variable&) const = default;
        bool operator<(const Variable& other) const { return name < other.name; }
        bool operator==(const std::string& other) const { return name == other; }
        bool operator<(const std::string& other) const { return name < other; }
    };

    class Number : public ASTNode {
        int64_t value;
    public:
        Number() : value(0) {}
        Number(int64_t v) : value(v) {}
        std::string to_string() const override { return std::to_string(value); }
        int64_t getValue() const { return value; }
    };

    enum class Register {
        rcx,
        rdi, rsi, rdx, r8, r9,
        rax, rbx, rbp, r10, r11, r12, r13, r14, r15,
        rsp
    };

    enum class AopType { AddEq, SubEq, MulEq, AndEq };
    enum class SopType { LShift, RShift };
    enum class CmpType { Eq, Neq, Lt, Lte, Gt, Gte };

    struct memoryAccess {
        std::variant<Register, Variable> base = Register::rsp;
        int64_t size = 0;
    };

    using VALUE = std::variant<memoryAccess, Register, Label, Number, Variable>;

    struct compareStruct {
        VALUE left;
        std::string cmp;
        VALUE right;
    };

    enum class InstructionType {
        Unknown,
        compareAssign, AssignFromMemory, AssignFromS, AssignMemoryFromS, AssignFromStack,
        WaopT, WsopSx, WsopN,
        WIncDec, MemoryIncDecT, WIncDecMemory,
        WAtWWE,
        CallPrint, CallInput, CallAllocate, CallTupleError, CallTensorError, CallUN,
        CJump, Goto, Label, Return
    };

    inline bool isAssignType(InstructionType t) {
        return t == InstructionType::compareAssign    ||
               t == InstructionType::AssignFromMemory ||
               t == InstructionType::AssignFromS      ||
               t == InstructionType::AssignMemoryFromS ||
               t == InstructionType::AssignFromStack;
    }

    // ---------- Shared string-conversion helpers ----------
    //
    // These were inline lambdas duplicated in every Instruction subclass.
    // Pulling them out here means there's exactly one place to fix bugs
    // or extend behavior when the IR grows.

    inline std::string registerToString(Register r) {
        switch (r) {
            case Register::rcx: return "rcx";
            case Register::rdi: return "rdi";
            case Register::rsi: return "rsi";
            case Register::rdx: return "rdx";
            case Register::r8:  return "r8";
            case Register::r9:  return "r9";
            case Register::rax: return "rax";
            case Register::rbx: return "rbx";
            case Register::rbp: return "rbp";
            case Register::r10: return "r10";
            case Register::r11: return "r11";
            case Register::r12: return "r12";
            case Register::r13: return "r13";
            case Register::r14: return "r14";
            case Register::r15: return "r15";
            case Register::rsp: return "rsp";
        }
        return "";
    }

    inline CmpType stringToCmpType(const std::string& cmp) {
        if (cmp == "=")  return CmpType::Eq;
        if (cmp == "!=") return CmpType::Neq;
        if (cmp == "<")  return CmpType::Lt;
        if (cmp == "<=") return CmpType::Lte;
        if (cmp == ">")  return CmpType::Gt;
        if (cmp == ">=") return CmpType::Gte;
        assert(false && "unknown cmp operator");
        return CmpType::Eq;
    }

    inline std::string aopToString(AopType a) {
        switch (a) {
            case AopType::AddEq: return "+=";
            case AopType::SubEq: return "-=";
            case AopType::MulEq: return "*=";
            case AopType::AndEq: return "&=";
        }
        return "";
    }

    inline std::string sopToString(SopType s) {
        switch (s) {
            case SopType::LShift: return "<<=";
            case SopType::RShift: return ">>=";
        }
        return "";
    }

    inline std::string memBaseToString(const memoryAccess& m) {
        return std::visit([](const auto& b) -> std::string {
            using T = std::decay_t<decltype(b)>;
            if constexpr (std::is_same_v<T, Register>) return registerToString(b);
            else                                       return b.name;  // Variable
        }, m.base);
    }

    // The one and only VALUE -> string function. Replaces every
    // per-class valueToString lambda.
    inline std::string valueToString(const VALUE& v) {
        return std::visit([](const auto& val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, Register>)     return registerToString(val);
            else if constexpr (std::is_same_v<T, Number>)  return val.to_string();
            else if constexpr (std::is_same_v<T, Label>)   return val.name;
            else if constexpr (std::is_same_v<T, Variable>) return val.name;
            else if constexpr (std::is_same_v<T, memoryAccess>)
                return "mem " + memBaseToString(val) + " " + std::to_string(val.size);
            else {
                assert(false && "unknown VALUE type");
                return "";
            }
        }, v);
    }

    // Convenience helper: if a VALUE is a Variable, return it wrapped in
    // a set; otherwise return empty. Used to build reads()/writes() sets.
    inline std::set<Variable> varsIn(const VALUE& v) {
        std::set<Variable> out;
        if (std::holds_alternative<Variable>(v)) {
            out.insert(std::get<Variable>(v));
        } else if (std::holds_alternative<memoryAccess>(v)) {
            const memoryAccess& m = std::get<memoryAccess>(v);
            if (std::holds_alternative<Variable>(m.base)) {
                out.insert(std::get<Variable>(m.base));
            }
        }
        return out;
    }

    // ---------- Instruction hierarchy ----------

    class Instruction : public ASTNode {
    public:
        InstructionType type;
        Instruction() = delete;
        Instruction(InstructionType t) : type(t) {}
        virtual ~Instruction() = default;

        bool verify() const override { return true; }

        // Every concrete instruction must say which variables it touches
        // and how to rewrite them. Default to "none" so instructions with
        // no variable operands (return, label, goto) don't need to override.
        virtual std::set<Variable> reads()  const { return {}; }
        virtual std::set<Variable> writes() const { return {}; }
        virtual void replaceVar(const Variable& /*from*/, const Variable& /*to*/) {}
    };

    class CjumpInstruction : public Instruction {
    public:
        std::optional<compareStruct> cmp_val;
        Label label;

        CjumpInstruction() : Instruction(InstructionType::CJump) {}

        void setCmpVal(compareStruct& cv) { cmp_val = std::move(cv); }
        void setLabel(const Label& lbl)   { label = lbl; }

        std::string to_string() const override {
            std::string result = "\tcjump ";
            result += valueToString(cmp_val->left);
            result += " " + cmp_val->cmp + " ";
            result += valueToString(cmp_val->right);
            result += " :" + label.name + "\n";
            return result;
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (cmp_val) {
                auto l  = varsIn(cmp_val->left);  r.insert(l.begin(), l.end());
                auto rr = varsIn(cmp_val->right); r.insert(rr.begin(), rr.end());
            }
            return r;
        }

        void replaceVar(const Variable& from, const Variable& to) override {
            if (!cmp_val) return;
            if (std::holds_alternative<Variable>(cmp_val->left) &&
                std::get<Variable>(cmp_val->left) == from) {
                cmp_val->left = to;
            }
            if (std::holds_alternative<Variable>(cmp_val->right) &&
                std::get<Variable>(cmp_val->right) == from) {
                cmp_val->right = to;
            }
        }
    };

    class AssignInstruction : public Instruction {
    public:
        std::optional<VALUE> from;
        std::optional<VALUE> to;
        std::optional<compareStruct> cmp_val;

        AssignInstruction() : Instruction(InstructionType::Unknown) {}
        AssignInstruction(InstructionType t) : Instruction(t) { assert(isAssignType(t)); }

        void setType(InstructionType t) { assert(isAssignType(t)); type = t; }
        void setFrom(VALUE v)           { from = std::move(v); }
        void setTo(VALUE v)             { to   = std::move(v); }
        void setCmpVal(compareStruct cv) { cmp_val = std::move(cv); }

        InstructionType getType() const { return type; }
        const std::optional<VALUE>& getFrom() const { return from; }
        const std::optional<VALUE>& getTo()   const { return to; }
        const std::optional<compareStruct>& getCmpVal() const { return cmp_val; }

        bool hasFrom() const { return from.has_value(); }
        bool hasTo()   const { return to.has_value(); }
        bool isCmpAssign() const { return type == InstructionType::compareAssign; }
        bool isComplete() const {
            return type != InstructionType::Unknown &&
                   (from.has_value() || cmp_val.has_value()) &&
                   to.has_value();
        }

        bool verify() const override { return isComplete(); }

        std::string to_string() const override {
            assert(isComplete());
            std::string result = "\t";

            switch (type) {
                case InstructionType::AssignFromS:
                case InstructionType::AssignFromMemory:
                case InstructionType::AssignMemoryFromS:
                    result += valueToString(to.value());
                    result += " <- ";
                    result += valueToString(from.value());
                    break;

                case InstructionType::compareAssign:
                    result += valueToString(to.value());
                    result += " <- ";
                    result += valueToString(cmp_val->left);
                    result += " " + cmp_val->cmp + " ";
                    result += valueToString(cmp_val->right);
                    break;

                default:
                    assert(false && "unknown assign type");
            }

            return result + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            // W <- mem X M and W <- S read the source.
            // mem X M <- S reads both the memory base and the source.
            if (from) {
                auto fr = varsIn(*from);
                r.insert(fr.begin(), fr.end());
            }
            if (type == InstructionType::AssignMemoryFromS && to) {
                // The destination is a memory access; its base is READ
                // (we're not overwriting the base register/variable).
                auto tr = varsIn(*to);
                r.insert(tr.begin(), tr.end());
            }
            if (cmp_val) {
                auto l  = varsIn(cmp_val->left);  r.insert(l.begin(), l.end());
                auto rr = varsIn(cmp_val->right); r.insert(rr.begin(), rr.end());
            }
            return r;
        }

        std::set<Variable> writes() const override {
            // Only W-destinations are written. Memory-store destinations
            // do not "write" a variable in the live-analysis sense.
            if (type == InstructionType::AssignMemoryFromS) return {};
            if (to) return varsIn(*to);
            return {};
        }

        void replaceVar(const Variable& fromV, const Variable& toV) override {
            auto replace_in = [&](VALUE& v) {
                if (std::holds_alternative<Variable>(v)) {
                    if (std::get<Variable>(v) == fromV) v = toV;
                } else if (std::holds_alternative<memoryAccess>(v)) {
                    memoryAccess& m = std::get<memoryAccess>(v);
                    if (std::holds_alternative<Variable>(m.base) &&
                        std::get<Variable>(m.base) == fromV) {
                        m.base = toV;
                    }
                }
            };
            if (from) replace_in(*from);
            if (to)   replace_in(*to);
            if (cmp_val) {
                if (std::holds_alternative<Variable>(cmp_val->left) &&
                    std::get<Variable>(cmp_val->left) == fromV) {
                    cmp_val->left = toV;
                }
                if (std::holds_alternative<Variable>(cmp_val->right) &&
                    std::get<Variable>(cmp_val->right) == fromV) {
                    cmp_val->right = toV;
                }
            }
        }
    };

    class CallInstruction : public Instruction {
    public:
        std::optional<VALUE> callee;
        std::optional<int64_t> arg;

        void setCallee(VALUE v) { callee = std::move(v); }
        void setNum(int64_t n)  { arg = n; }

        CallInstruction(InstructionType t, int64_t argument = 0) : Instruction(t) {
            assert(t == InstructionType::CallPrint       ||
                   t == InstructionType::CallInput       ||
                   t == InstructionType::CallAllocate    ||
                   t == InstructionType::CallTupleError  ||
                   t == InstructionType::CallTensorError ||
                   t == InstructionType::CallUN);

            switch (t) {
                case InstructionType::CallPrint:       arg = 1; break;
                case InstructionType::CallInput:       arg = 0; break;
                case InstructionType::CallAllocate:    arg = 2; break;
                case InstructionType::CallTensorError: arg = -1; break;
                case InstructionType::CallUN:          arg = argument; break;
                default: break;
            }
        }

        std::string to_string() const override {
            switch (type) {
                case InstructionType::CallPrint:       return "\tcall print 1\n";
                case InstructionType::CallInput:       return "\tcall input 0\n";
                case InstructionType::CallAllocate:    return "\tcall allocate 2\n";
                case InstructionType::CallTupleError:  return "\tcall tuple-error 3\n";
                case InstructionType::CallTensorError: return "\tcall tensor-error\n";
                case InstructionType::CallUN:
                    return "\tcall " + (callee ? valueToString(*callee) : std::string{})
                         + " " + std::to_string(arg.value()) + "\n";
                default: return "";
            }
        }

        std::set<Variable> reads() const override {
            // The callee itself might be a variable (call %f N).
            if (callee) return varsIn(*callee);
            return {};
        }

        void replaceVar(const Variable& from, const Variable& to) override {
            if (callee &&
                std::holds_alternative<Variable>(*callee) &&
                std::get<Variable>(*callee) == from) {
                *callee = to;
            }
        }
    };

    class ReturnInstruction : public Instruction {
    public:
        ReturnInstruction() : Instruction(InstructionType::Return) {}
        std::string to_string() const override { return "\treturn\n"; }
    };

    class LabelInstruction : public Instruction {
    public:
        Label label;

        LabelInstruction(Label _label)
            : Instruction(InstructionType::Label), label(std::move(_label)) {}

        std::string to_string() const override {
            return "\t" + label.name + "\n";
        }
    };

    class GotoInstruction : public Instruction {
    public:
        Label label;

        GotoInstruction(Label _label)
            : Instruction(InstructionType::Goto), label(std::move(_label)) {}

        std::string to_string() const override {
            return "\tgoto " + label.name + "\n";
        }
    };

    class ArithInstruction : public Instruction {
    public:
        std::optional<VALUE> dst;
        AopType aop;
        std::optional<VALUE> src;

        ArithInstruction(InstructionType t) : Instruction(t) {}

        void setDst(VALUE v)   { dst = std::move(v); }
        void setSrc(VALUE v)   { src = std::move(v); }
        void setAop(AopType a) { aop = a; }

        const std::optional<VALUE>& getDst() const { return dst; }
        const std::optional<VALUE>& getSrc() const { return src; }
        AopType getAop() const                     { return aop; }

        std::string to_string() const override {
            return "\t" + valueToString(dst.value()) + " "
                 + aopToString(aop) + " "
                 + valueToString(src.value()) + "\n";
        }

        std::set<Variable> reads() const override {
            // W op= t reads BOTH dst (we need the old value) and src.
            std::set<Variable> r;
            if (dst) { auto d = varsIn(*dst); r.insert(d.begin(), d.end()); }
            if (src) { auto s = varsIn(*src); r.insert(s.begin(), s.end()); }
            return r;
        }

        std::set<Variable> writes() const override {
            if (dst) return varsIn(*dst);
            return {};
        }

        void replaceVar(const Variable& from, const Variable& to) override {
            auto replace_in = [&](VALUE& v) {
                if (std::holds_alternative<Variable>(v)) {
                    if (std::get<Variable>(v) == from) v = to;
                } else if (std::holds_alternative<memoryAccess>(v)) {
                    memoryAccess& m = std::get<memoryAccess>(v);
                    if (std::holds_alternative<Variable>(m.base) &&
                        std::get<Variable>(m.base) == from) {
                        m.base = to;
                    }
                }
            };
            if (dst) replace_in(*dst);
            if (src) replace_in(*src);
        }
    };

    class ShiftInstruction : public Instruction {
    public:
        std::optional<VALUE> dst;
        SopType sop;
        std::optional<VALUE> src;

        ShiftInstruction(InstructionType t) : Instruction(t) {}

        void setDst(VALUE v)   { dst = std::move(v); }
        void setSrc(VALUE v)   { src = std::move(v); }
        void setSop(SopType s) { sop = s; }

        std::string to_string() const override {
            return "\t" + valueToString(dst.value()) + " "
                 + sopToString(sop) + " "
                 + valueToString(src.value()) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (dst) { auto d = varsIn(*dst); r.insert(d.begin(), d.end()); }
            if (src) { auto s = varsIn(*src); r.insert(s.begin(), s.end()); }
            return r;
        }

        std::set<Variable> writes() const override {
            if (dst) return varsIn(*dst);
            return {};
        }

        void replaceVar(const Variable& from, const Variable& to) override {
            if (dst &&
                std::holds_alternative<Variable>(*dst) &&
                std::get<Variable>(*dst) == from) {
                *dst = to;
            }
            if (src &&
                std::holds_alternative<Variable>(*src) &&
                std::get<Variable>(*src) == from) {
                *src = to;
            }
        }
    };

    class IncDecInstruction : public Instruction {
    public:
        std::optional<VALUE> dst;
        bool isIncrement;

        IncDecInstruction() : Instruction(InstructionType::WIncDec) {}
        void setDst(VALUE v)    { dst = std::move(v); }
        void setIsInc(bool inc) { isIncrement = inc; }

        std::string to_string() const override {
            return "\t" + valueToString(dst.value())
                 + (isIncrement ? "++" : "--") + "\n";
        }

        std::set<Variable> reads() const override {
            // ++ and -- read the current value of dst.
            if (dst) return varsIn(*dst);
            return {};
        }

        std::set<Variable> writes() const override {
            if (dst) return varsIn(*dst);
            return {};
        }

        void replaceVar(const Variable& from, const Variable& to) override {
            if (dst &&
                std::holds_alternative<Variable>(*dst) &&
                std::get<Variable>(*dst) == from) {
                *dst = to;
            }
        }
    };

    class WWWEInstruction : public Instruction {
    public:
        std::optional<VALUE> dst;
        std::optional<VALUE> base;
        std::optional<VALUE> idx;
        int64_t scale;

        WWWEInstruction() : Instruction(InstructionType::WAtWWE) {}
        void setDst(VALUE v)     { dst  = std::move(v); }
        void setBase(VALUE v)    { base = std::move(v); }
        void setIdx(VALUE v)     { idx  = std::move(v); }
        void setScale(int64_t s) { scale = s; }

        std::string to_string() const override {
            return "\t" + valueToString(dst.value())  + " @ "
                 + valueToString(base.value()) + " "
                 + valueToString(idx.value())  + " "
                 + std::to_string(scale) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            if (base) { auto b = varsIn(*base); r.insert(b.begin(), b.end()); }
            if (idx)  { auto i = varsIn(*idx);  r.insert(i.begin(), i.end()); }
            return r;
        }

        std::set<Variable> writes() const override {
            if (dst) return varsIn(*dst);
            return {};
        }

        void replaceVar(const Variable& from, const Variable& to) override {
            auto replace = [&](std::optional<VALUE>& o) {
                if (!o) return;
                if (std::holds_alternative<Variable>(*o) &&
                    std::get<Variable>(*o) == from) {
                    *o = to;
                }
            };
            replace(dst);
            replace(base);
            replace(idx);
        }
    };

    class MemIncDecInstruction : public Instruction {
    public:
        memoryAccess mem;
        AopType aop;
        std::optional<VALUE> src;

        MemIncDecInstruction() : Instruction(InstructionType::MemoryIncDecT) {}
        void setMem(memoryAccess m) { mem = m; }
        void setAop(AopType a)      { aop = a; }
        void setSrc(VALUE v)        { src = std::move(v); }

        std::string to_string() const override {
            return "\tmem " + memBaseToString(mem) + " "
                 + std::to_string(mem.size) + " "
                 + aopToString(aop) + " "
                 + valueToString(src.value()) + "\n";
        }

        std::set<Variable> reads() const override {
            std::set<Variable> r;
            // The memory base is read (we need it to form the address).
            if (std::holds_alternative<Variable>(mem.base)) {
                r.insert(std::get<Variable>(mem.base));
            }
            if (src) { auto s = varsIn(*src); r.insert(s.begin(), s.end()); }
            return r;
        }

        // This instruction writes memory, not a variable, so writes() is empty.

        void replaceVar(const Variable& from, const Variable& to) override {
            if (std::holds_alternative<Variable>(mem.base) &&
                std::get<Variable>(mem.base) == from) {
                mem.base = to;
            }
            if (src &&
                std::holds_alternative<Variable>(*src) &&
                std::get<Variable>(*src) == from) {
                *src = to;
            }
        }
    };

    // ---------- Function / Program ----------

    class Function : public ASTNode {
        Label label;
    public:
        std::vector<std::unique_ptr<Instruction>> instructions;
        int num_args = 0;
        int num_locals = 0;      // stack slots reserved for spilled variables
        bool args_set = false;

        Function() = default;

        std::string to_string() const override {
            std::string result;
            result += "\t" + label.name + "\n\t";
            result += std::to_string(num_args) + "\n";
            for (auto& instruction : instructions) {
                result += instruction->to_string();
            }
            result += "\t)\n";
            return result;
        }

        std::string getLabel() const { return label.name; }
        int getNumArgs()       const { return num_args; }

        void setLabel(std::string l) { label = Label(std::move(l)); }
        void setNumArgs(int n)       { args_set = true; num_args = n; }

        bool verify() const override {
            return args_set && !instructions.empty();
        }
    };

    class Program : public ASTNode {
    public:
        Label label;
        std::vector<Function> functions;

        Program() = default;

        std::string to_string() const override {
            std::string result;
            result += "(" + label.name + "\n";
            for (auto& function : functions) {
                result += function.to_string();
            }
            result += ")";
            return result;
        }

        bool verify() const override {
            for (auto& function : functions) {
                if (function.getLabel() == label.name) return true;
            }
            std::cerr << "no matching function with a program label\n";
            return false;
        }
    };

} // namespace L2