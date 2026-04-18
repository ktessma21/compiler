#include <fstream>
#include "l1.h"
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <cassert>



namespace pegtl = TAO_PEGTL_NAMESPACE;


#define pstring TAO_PEGTL_STRING

using namespace pegtl;


// handle the M case especially using code generation step. for now assume M is number

namespace L1 {


 /*
   * Grammar rules from now on.
   */
  struct name:
    pegtl::seq<
      pegtl::plus<
        pegtl::sor<
          pegtl::alpha,
          pegtl::one< '_' >
        >
      >,
      pegtl::star<
        pegtl::sor<
          pegtl::alpha,
          pegtl::one< '_' >,
          pegtl::digit
        >
      >
    > {};


  struct number:
    pegtl::seq<
      pegtl::opt<
        pegtl::sor<
          pegtl::one< '-' >,
          pegtl::one< '+' >
        >
      >,
      pegtl::plus<
        pegtl::digit
      >
    >{};


    struct comment:
        pegtl::disable<
        pstring( "//" ),
        pegtl::until< pegtl::eolf >
         >{};

    struct spaces :
        pegtl::star<
            pegtl::sor<
                pegtl::one< ' ' >,
                pegtl::one< '\t'>
            >
        > { };

    struct seps :
        pegtl::star<
            pegtl::seq<
                spaces,
                pegtl::eol
            >
        > { };

    struct seps_with_comments :
        pegtl::star<
            pegtl::seq<
                spaces,
                pegtl::sor<
                    pegtl::eol,
                    comment
                >
            >
        > { };




        /* MY CODE GOES HERE */
struct entry_point_rule; // early declaration.
struct l;

struct sx: pstring("rcx"){};

struct a :
    pegtl::sor<
            sx,
            pstring("rdi"),
            pstring("rsi"),
            pstring("rdx"),
            pstring("r8"),
            pstring("r9")
    > {};


struct W :
        pegtl::sor<
            a,
            pstring("rax"),
            pstring("rbx"),
            pstring("rbp"),
            pstring("r10"),
            pstring("r11"),
            pstring("r12"),
            pstring("r13"),
            pstring("r14"),
            pstring("r15")
    > {};

struct aop :
    pegtl::sor<
        pstring("+="),
        pstring("-="),
        pstring("*="),
        pstring("&=")
    > {};

struct sop :
    pegtl::sor<
        pstring("<<="),
        pstring(">>=")
    > {};

struct cmp :
    pegtl::sor<
        pstring("<="),
        pstring("<"),
        pstring("=")
    > {};

struct E :
    pegtl::sor<
        pegtl::one<'8'>,
        pegtl::one<'4'>,
        pegtl::one<'2'>,
        pegtl::one<'1'>
    > {};

struct F :
    pegtl::sor<
        pegtl::one<'4'>,
        pegtl::one<'3'>,
        pegtl::one<'1'>
    > {};

struct label :
        pegtl::seq<
            pegtl::one<':'>,
            name
        > {};

struct label_instruction_block :
        pegtl::seq<
            pegtl::one<':'>,
            name
        > {};

struct u :
        pegtl::sor<
            W,
            l
        > {};

struct X :
        pegtl::sor<
            W,
            pstring("rsp")
        > {};

struct t :
        pegtl::sor <
            X,
            number
        > {};

struct S :
    pegtl::sor<
        l,
        label,
        t
    >{};

struct M : number {};


/* All instructions set defined from here */
struct memory_access_block :
    pegtl::seq<
        pstring("mem"),
                spaces,
                X,
                spaces,
                M
    >{};


struct assignWfromMemory:
        pegtl::seq<
            W,
            spaces,
            pstring("<-"),
            spaces,
            memory_access_block
        >{};

struct assignWfromS :
        pegtl::seq<
            W,
            spaces,
            pstring("<-"),
            spaces,
            S
        >{};

struct WaopT:
    pegtl::seq<
            W,
            spaces,
            aop,
            spaces,
            t
        >{};

struct WsopSx:
    pegtl::seq<
                W,
                spaces,
                sop,
                spaces,
                sx
            >{};

struct WsopN:
    pegtl::seq<
                W,
                spaces,
                sop,
                spaces,
                number
            >{};

struct memoryIncDecT:
    pegtl::seq<
        memory_access_block,
        spaces,
        pegtl::sor<
            pstring("+="),
            pstring("-=")>,
        spaces,
        t
    >{};

struct wIncDecMemory:
    pegtl::seq<
        W,
        spaces,
        pegtl::sor<
            pstring("+="),
            pstring("-=")>,
        spaces,
        memory_access_block
    >{};

struct compareAssign:
    pegtl::seq<
        W,
        spaces,
        pstring("<-"),
        spaces,
        t,
        spaces,
        cmp,
        spaces,
        t
    >{};

struct cjump:
    pegtl::seq<
        pstring("cjump"),
        spaces,
        t,
        spaces,
        cmp,
        spaces,
        t,
        spaces,
        label
    >{};


struct gotoLabel:
    pegtl::seq<
        pstring("goto"),
        spaces,
        label
    >{};


struct callPrint:
    pegtl::seq<
            pstring("print"),
            spaces,
            pegtl::one<'1'>
    >{};

struct callUN:
    pegtl::seq<
        u,
        spaces,
        number
    >{};

struct callInput:
    pegtl::seq<
            pstring("input"),
            spaces,
            pegtl::one<'0'>
    >{};

struct callAllocate:
    pegtl::seq<
            pstring("allocate"),
            spaces,
            pegtl::one<'2'>
    >{};


struct calltupleError:
    pegtl::seq<
            pstring("tuple-error"),
            spaces,
            pegtl::one<'3'>
    >{};

struct calltensorError:
    pegtl::seq<
            pstring("tensor-error"),
            spaces,
            F
    >{};

struct callInstruction_block :
    pegtl::seq<
        pstring("call"),
        spaces,
        pegtl::sor<
            callUN,   // call u number       (not generic actually fallback)
            callPrint,        // call print 1        (specific string)
            callInput,        // call input 0        (specific string)
            callAllocate,     // call allocate 2     (specific string)
            calltupleError,   // call tuple-error 3  (specific string)
            calltensorError
        >
    >{};

struct wIncDec:
    pegtl::seq<
            W,
            spaces,
            pegtl::sor<
                pstring("++"),
                pstring("--")>
    >{};

struct wAtWWE:
    pegtl::seq<
        W,
        spaces,
        pegtl::one<'@'>,
        spaces,
        W,
        spaces,
        W,
        spaces,
        E
    >{};

struct assignMemoryFromS :
    pegtl::seq<
        memory_access_block,
        spaces,
        pstring("<-"),
        spaces,
        S
    >{};


struct returnINS : pstring("return"){};

struct assignment_block :
        pegtl::sor<
            compareAssign,      // W <- t cmp t   (before assignWfromS)
            assignWfromMemory,  // W <- mem ...   (before assignWfromS, "mem" is specific)
            assignWfromS,       // W <- S         (generic <- fallback)
            assignMemoryFromS> {};

struct Instruction_block :
        pegtl::sor<
            assignment_block,
            wIncDecMemory,      // W op= mem ...  (before WaopT, "mem" is specific)
            WsopSx,             // W sop sx       (before WsopN, sx is specific)
            WsopN,              // W sop number   (sop fallback)
            wIncDec,
            WaopT,              // W aop t        (generic aop)
            wAtWWE,
            memoryIncDecT,      // mem X M op= t  (unique prefix)
            callInstruction_block,
            cjump,              // cjump t cmp t label
            label_instruction_block,                // just label
            gotoLabel,
            returnINS           // return         (unique)
        >{};





// Don't touch from now on. Extremely stable parsing code.
struct InstructionFormat :
    pegtl::seq<
        spaces,
        Instruction_block,
        seps_with_comments>
    {};


struct functionFormat :
    pegtl::seq<
        number,
        seps_with_comments,
        spaces, // allow new lines too
        number,
        seps_with_comments,
        pegtl::plus<InstructionFormat>
    > {};


struct programORfunction :
    pegtl::seq<
        spaces,
        pegtl::sor<
            pegtl::plus<functionFormat>,       // multiple functions ← fix
            pegtl::plus<entry_point_rule>   // nested programs
        >
    >{};


// clean entry format
    struct l :
        pegtl::seq<
            pegtl::one<'@'>,
            name> {};

    struct entry_point_rule:
        pegtl::seq<
            seps_with_comments,
            pegtl::seq<spaces, pegtl::one< '(' >>,
            l,
            seps_with_comments,
            programORfunction,      // it used to be a program, so now wait for function from now on. or it's function wait for number
            pegtl::seq<spaces, pegtl::one< ')' >>,
            seps_with_comments
        > { };

    struct grammar :
        pegtl::must<
            entry_point_rule
        > {};


    /* ------------------------------------------------------------------
     *  Tokenizer — splits a string on whitespace and hands out tokens
     *  one at a time. Replaces all the manual pos++ / find / substr
     *  scaffolding the actions used to do by hand.
     * ------------------------------------------------------------------ */
    class Tokenizer {
        std::vector<std::string> tokens;
        size_t pos = 0;

    public:
        explicit Tokenizer(const std::string& s) {
            std::string cur;
            for (char c : s) {
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
                } else {
                    cur += c;
                }
            }
            if (!cur.empty()) tokens.push_back(cur);
        }

        // get next token and advance
        std::string next() {
            if (pos >= tokens.size())
                throw std::runtime_error("tokenizer: no more tokens");
            return tokens[pos++];
        }

        // look without advancing
        const std::string& peek(size_t offset = 0) const {
            if (pos + offset >= tokens.size())
                throw std::runtime_error("tokenizer: peek past end");
            return tokens[pos + offset];
        }

        bool done() const { return pos >= tokens.size(); }
        size_t remaining() const { return tokens.size() - pos; }

        // consume and verify (useful for "mem", "<-", etc.)
        void expect(const std::string& s) {
            std::string t = next();
            if (t != s)
                throw std::runtime_error("tokenizer: expected '" + s + "' got '" + t + "'");
        }
    };


    inline Register stringToRegister(const std::string& s) {
        if (s == "rcx") return Register::rcx;
        if (s == "rdi") return Register::rdi;
        if (s == "rsi") return Register::rsi;
        if (s == "rdx") return Register::rdx;
        if (s == "r8")  return Register::r8;
        if (s == "r9")  return Register::r9;
        if (s == "rax") return Register::rax;
        if (s == "rbx") return Register::rbx;
        if (s == "rbp") return Register::rbp;
        if (s == "r10") return Register::r10;
        if (s == "r11") return Register::r11;
        if (s == "r12") return Register::r12;
        if (s == "r13") return Register::r13;
        if (s == "r14") return Register::r14;
        if (s == "r15") return Register::r15;
        if (s == "rsp") return Register::rsp;
        throw std::runtime_error("unknown register: " + s);
    }

   inline AopType parseAop(const std::string& s) {
    if (s == "+=") return AopType::AddEq;
    if (s == "-=") return AopType::SubEq;
    if (s == "*=") return AopType::MulEq;
    return AopType::AndEq;  // &=
    }

    inline SopType parseSop(const std::string& s){
        if (s == "<<=") return SopType::LShift;
        return SopType::RShift;
    }

    inline VALUE parseS(const std::string& str) {
        if (!str.empty() && str[0] == '@') return Label(str);
        if (!str.empty() && str[0] == ':') return Label(str);
        try {
            return stringToRegister(str);
        } catch (...) {
            return Number(std::stoll(str));
        }
    }

    inline VALUE parseT(const std::string& str){
        if (!str.empty() && str[0] == 'r'){
            return stringToRegister(str);

        }else{
            return Number(std::stoll(str));
        }
    }

    /* FUll ITEM collecting */
    bool new_function = false;


    template<typename Rule>
    struct action : pegtl::normal<Rule> {};


    template<> struct action <wIncDec> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // wIncDec has no space between reg and ++/-- (e.g. "rax++").
            // Tokenizer would give us one blob, so split that blob.
            Tokenizer tk(in.string());
            std::string blob = tk.next();

            std::string reg_str;
            bool is_inc;
            if (blob.size() >= 2 && blob.substr(blob.size()-2) == "++") {
                reg_str = blob.substr(0, blob.size()-2);
                is_inc = true;
            } else if (blob.size() >= 2 && blob.substr(blob.size()-2) == "--") {
                reg_str = blob.substr(0, blob.size()-2);
                is_inc = false;
            } else {
                // tolerate the case where there *is* whitespace:
                // blob was just the register, next token is ++/--
                reg_str = blob;
                std::string op = tk.next();
                if (op == "++") is_inc = true;
                else if (op == "--") is_inc = false;
                else throw std::runtime_error("invalid increment/decrement: " + in.string());
            }

            auto instr = std::make_unique<IncDecInstruction>();
            instr->setDst(stringToRegister(reg_str));
            instr->setIsInc(is_inc);
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<wAtWWE> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            Tokenizer tk(in.string());
            std::string dst_str  = tk.next();   // W
            tk.expect("@");
            std::string base_str = tk.next();   // W
            std::string idx_str  = tk.next();   // W
            std::string e_str    = tk.next();   // E

            auto instr = std::make_unique<WWWEInstruction>();
            instr->setDst(stringToRegister(dst_str));
            instr->setBase(stringToRegister(base_str));
            instr->setIdx(stringToRegister(idx_str));
            instr->setScale(std::stoll(e_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<WaopT> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string aop_str = tk.next();
            std::string t_str   = tk.next();

            auto instr = std::make_unique<ArithInstruction>(InstructionType::WaopT);
            instr->setDst(stringToRegister(w_str));
            instr->setAop(parseAop(aop_str));
            instr->setSrc(parseT(t_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };


    template<> struct action<WsopN> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string sop_str = tk.next();
            std::string n_str   = tk.next();

            auto instr = std::make_unique<ShiftInstruction>(InstructionType::WsopN);
            instr->setDst(stringToRegister(w_str));
            instr->setSop(parseSop(sop_str));
            instr->setSrc(Number(std::stoll(n_str)));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<wIncDecMemory> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  W aop mem X M
            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string aop_str = tk.next();
            tk.expect("mem");
            std::string x_str   = tk.next();
            std::string m_str   = tk.next();

            memoryAccess m;
            m.x_value = stringToRegister(x_str);
            m.size    = std::stoll(m_str);

            auto instr = std::make_unique<ArithInstruction>(InstructionType::WIncDecMemory);
            instr->setDst(stringToRegister(w_str));
            instr->setAop(parseAop(aop_str));
            instr->setSrc(VALUE(m));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<cjump> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  cjump t cmp t :label
            Tokenizer tk(in.string());
            tk.expect("cjump");
            std::string left_str  = tk.next();
            std::string cmp_str   = tk.next();
            std::string right_str = tk.next();
            std::string label_tok = tk.next();   // ":name"

            // strip leading ':'
            std::string label_str = (!label_tok.empty() && label_tok[0] == ':')
                                    ? label_tok.substr(1) : label_tok;

            compareStruct cav;
            cav.left  = std::make_unique<VALUE>(parseT(left_str));
            cav.cmp   = cmp_str;
            cav.right = std::make_unique<VALUE>(parseT(right_str));

            auto instr = std::make_unique<CjumpInstruction>();
            instr->setCmpVal(cav);
            instr->setLabel(Label(label_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<WsopSx> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string sop_str = tk.next();
            // sx is always rcx; consume it for sanity
            (void)tk.next();

            auto instr = std::make_unique<ShiftInstruction>(InstructionType::WsopSx);
            instr->setDst(stringToRegister(w_str));
            instr->setSop(parseSop(sop_str));
            instr->setSrc(Register::rcx);
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<returnINS> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            auto ret = std::make_unique<ReturnInstruction>();
            p.functions.back().instructions.push_back(std::move(ret));
        }
    };

    template<> struct action<callInstruction_block> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  call <callee> <num>   OR   call <builtin> <num>
            Tokenizer tk(in.string());
            tk.expect("call");
            std::string u_str = tk.next();

            auto classify = [](const std::string& s) -> InstructionType {
                if (s.starts_with("@") || s.starts_with("r")) return InstructionType::CallUN;
                if (s.contains("print"))        return InstructionType::CallPrint;
                if (s.contains("input"))        return InstructionType::CallInput;
                if (s.contains("allocate"))     return InstructionType::CallAllocate;
                if (s.contains("tuple-error"))  return InstructionType::CallTupleError;
                if (s.contains("tensor-error")) return InstructionType::CallTensorError;
                return InstructionType::Unknown;
            };

            auto call = std::make_unique<CallInstruction>(classify(u_str));
            assert(call->type != InstructionType::Unknown);

            if (call->type == InstructionType::CallUN) {
                auto parseCallee = [](const std::string& str) -> VALUE {
                    if (str[0] == '@') return Label(str.substr(1));
                    return stringToRegister(str);
                };
                call->setCallee(parseCallee(u_str));
                std::string n_str = tk.next();
                call->setNum(std::stoll(n_str));
            }

            p.functions.back().instructions.push_back(std::move(call));
        }
    };


    template<> struct action<assignMemoryFromS> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  mem X M <- S
            Tokenizer tk(in.string());
            tk.expect("mem");
            std::string x_str = tk.next();
            std::string m_str = tk.next();
            tk.expect("<-");
            std::string s_str = tk.next();

            auto localParseS = [](const std::string& str) -> VALUE {
                if (str[0] == '@') return Label(str.substr(1));
                if (str[0] == ':') return Label(str.substr(1));
                try { return stringToRegister(str); }
                catch (...) { return Number(std::stoll(str)); }
            };

            memoryAccess m;
            m.x_value = stringToRegister(x_str);
            m.size    = std::stoll(m_str);

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignMemoryFromS);
            assign->setTo(VALUE(m));
            assign->setFrom(localParseS(s_str));
            p.functions.back().instructions.push_back(std::move(assign));
        }
    };

    template<> struct action <label_instruction_block> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  :name
            Tokenizer tk(in.string());
            std::string tok = tk.next();
            std::string label_name = (!tok.empty() && tok[0] == ':')
                                     ? tok.substr(1) : tok;

            auto lbl = std::make_unique<LabelInstruction>(label_name);
            p.functions.back().instructions.push_back(std::move(lbl));
        }
    };

    template<> struct action <gotoLabel> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  goto :name
            Tokenizer tk(in.string());
            tk.expect("goto");
            std::string tok = tk.next();
            std::string label_name = (!tok.empty() && tok[0] == ':')
                                     ? tok.substr(1) : tok;

            auto lbl = std::make_unique<GotoInstruction>(label_name);
            p.functions.back().instructions.push_back(std::move(lbl));
        }
    };

    template<> struct action<assignWfromS> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  W <- S
            Tokenizer tk(in.string());
            std::string w_str = tk.next();
            tk.expect("<-");
            std::string s_str = tk.next();

            auto localParseS = [](const std::string& str) -> VALUE {
                if (str[0] == '@') return Label(str.substr(1));
                if (str[0] == ':') return Label(str.substr(1));
                try { return stringToRegister(str); }
                catch (...) { return Number(std::stoll(str)); }
            };

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignFromS);
            assign->setTo(stringToRegister(w_str));
            assign->setFrom(localParseS(s_str));
            p.functions.back().instructions.push_back(std::move(assign));
        }
    };

    template<> struct action<compareAssign> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  W <- t cmp t
            Tokenizer tk(in.string());
            std::string w_str     = tk.next();
            tk.expect("<-");
            std::string left_str  = tk.next();
            std::string cmp_str   = tk.next();
            std::string right_str = tk.next();

            compareStruct cav;
            cav.left  = std::make_unique<VALUE>(parseT(left_str));
            cav.cmp   = cmp_str;
            cav.right = std::make_unique<VALUE>(parseT(right_str));

            auto assign = std::make_unique<AssignInstruction>(InstructionType::compareAssign);
            assign->setTo(stringToRegister(w_str));
            assign->setCmpVal(std::move(cav));
            p.functions.back().instructions.push_back(std::move(assign));
        }
    };

    template<> struct action < assignWfromMemory > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  W <- mem X M
            Tokenizer tk(in.string());
            std::string w_str = tk.next();
            tk.expect("<-");
            tk.expect("mem");
            std::string x_str = tk.next();
            std::string m_str = tk.next();

            memoryAccess m;
            m.x_value = stringToRegister(x_str);
            m.size    = std::stoll(m_str);

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignFromMemory);
            assign->setTo(stringToRegister(w_str));
            assign->setFrom(VALUE(m));
            p.functions.back().instructions.push_back(std::move(assign));
        }
    };

    template<> struct action<memoryIncDecT> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            // form:  mem X M aop t
            Tokenizer tk(in.string());
            tk.expect("mem");
            std::string x_str   = tk.next();
            std::string m_str   = tk.next();
            std::string aop_str = tk.next();
            std::string t_str   = tk.next();

            memoryAccess m;
            m.x_value = stringToRegister(x_str);
            m.size    = std::stoll(m_str);

            auto instr = std::make_unique<MemIncDecInstruction>();
            instr->setMem(m);
            instr->setAop(parseAop(aop_str));
            instr->setSrc(parseT(t_str));
            p.functions.back().instructions.push_back(std::move(instr));
        }
    };



    template<> struct action < l > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            // form:  @name
            std::string s = in.string();
            size_t at = s.find('@');
            assert(at != std::string::npos);
            std::string str_label = s.substr(at + 1);

            if (p.label.empty()){
                p.label = str_label;
                return;
            }
            if (!new_function){
                new_function = true;
                p.functions.push_back(Function()); // there must be at least one function
                p.functions.back().setLabel(str_label);
            }
        }
    };


    template<> struct action < functionFormat > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            new_function = false;

            // form begins with:  <num_args> <num_locals> ...
            Tokenizer tk(in.string());
            std::string num_args_str  = tk.next();
            std::string num_local_str = tk.next();

            p.functions.back().setNumArgs(std::stoll(num_args_str));
            p.functions.back().setNumLocals(std::stoll(num_local_str));
        }
    };

    template<> struct action < entry_point_rule > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            if (p.label.empty()){
                return;  // this is just a program
            }
            // else it must be a function
            // this would be okay because there is no nested functions
            // current_function = &p.functions.back(); dangerous idea because when the push_back needs more space
            // it will reallocate the entire to another address so the pointer will be dangling.
            return;
        }
    };


    bool TRACE = false;

    template<typename Rule>
    struct my_tracer : pegtl::normal<Rule> {
        template<typename Input, typename... States>
        static void start(const Input& in, States&&...) {
            if (!TRACE) return;
            std::cerr << "try   " << pegtl::demangle<Rule>()
                    << " at line " << in.position().line
                    << " col " << in.position().column << "\n";
        }
        template<typename Input, typename... States>
        static void success(const Input& in, States&&...) {
            if (!TRACE) return;
            std::cerr << "ok    " << pegtl::demangle<Rule>() << "\n";
        }
        template<typename Input, typename... States>
        static void failure(const Input& in, States&&...) {
            if (!TRACE) return;
            std::cerr << "FAIL  " << pegtl::demangle<Rule>() << "\n";
        }
    };


    Program parse_file (char *fileName){

        FILE *file = fopen(fileName, "r");

        if (!file){
            std::cerr << fileName << " : file not found." << std::endl;
            exit(1);
        }

        /*
        * Check the grammar for some possible issues.
        */
        if (pegtl::analyze< grammar >() != 0){
          std::cerr << "There are problems with the grammar" << std::endl;
          exit(1);
        }

        /*
        * Parse.
        */
        file_input< > fileInput(fileName);
        Program p;
        parse<grammar, action, my_tracer>(fileInput, p);

        return p;
    }
}