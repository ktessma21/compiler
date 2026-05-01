#include <fstream>
#include "l2.h"
#include <parser.h>
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

namespace L2 {


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

        
    struct var : 
            pegtl::seq<
                pegtl::one<'%'>,
                name
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

struct l;

struct sx: 
    pegtl::sor<
        pstring("rcx"), 
        var
    >{};

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
            pstring("rax")
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
struct stackArg :
    pegtl::seq<
        pstring("stack-arg"),
        spaces,
        M
    >{};

struct wFromStackArg :
    pegtl::seq<
        W,
        spaces,
        pstring("<-"),
        spaces,
        stackArg
    >{};

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

struct wStackM :
    pegtl::seq<
        W,
        spaces, 
        pstring("<-"), 
        spaces,
        var, 
        spaces,
        M
    >{};
        



struct returnINS : pstring("return"){};

struct assignment_block :
        pegtl::sor<
            wFromStackArg,
            wStackM,
            compareAssign,      // W <- t cmp t   (before assignWfromS)
            assignWfromMemory,  // W <- mem ...   (before assignWfromS, "mem" is specific)
            assignWfromS,       // W <- S         (generic <- fallback)
            assignMemoryFromS
            > {};

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
        spaces,
        number,     // in l2 we only have one number 
        seps_with_comments,
        pegtl::plus<InstructionFormat>
    > {};


// clean entry format
    struct l :
        pegtl::seq<
            pegtl::one<'@'>,
            name> {};

    struct function_def:
        pegtl::seq<
            seps_with_comments,
            pegtl::seq<spaces, pegtl::one< '(' >>,
            l,
            seps_with_comments,
            functionFormat,      // it used to be a Function, so now wait for function from now on. or it's function wait for number
            pegtl::seq<spaces, pegtl::one< ')' >>,
            seps_with_comments
        > { };

    struct entry_point_rule:
        pegtl::seq<
            seps_with_comments,
            pegtl::seq<spaces, pegtl::one< '(' >>,
            l,
            seps_with_comments,
            pegtl::plus<function_def>,      // it used to be a Function, so now wait for function from now on. or it's function wait for number
            pegtl::seq<spaces, pegtl::one< ')' >>,
            seps_with_comments
        > { };

    struct grammar_function :
        pegtl::must<
            function_def
        > {};

    struct grammar_program :
        pegtl::must<
            entry_point_rule
        > {};
    


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
        if (!str.empty() && str[0] == '%') return Variable(str);
        try {
            return stringToRegister(str);
        } catch (...) {
            return Number(std::stoll(str));
        }
    }

    inline VALUE parseT(const std::string& str){
        if (str.empty()) throw std::runtime_error("parseT: empty");
        if (str[0] == '%') return Variable(str);         // variable (using Label to carry it for now)
        if (str[0] == 'r') return stringToRegister(str);
        return Number(std::stoll(str));
    }

    inline VALUE parseW(const std::string& s) {
        if (!s.empty() && s[0] == '%') return Variable(s);   // variable
        return stringToRegister(s);                           // register
    }



    template<typename Rule>
    struct action : pegtl::normal<Rule> {};

    template<> struct action <entry_point_rule> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
           
            std::string s = in.string();
            size_t pos = s.find('(');  // outer program '('
            pos++;

            pos = s.find('@', pos);    // start of entry label
            size_t label_end = pos;
            while (label_end < s.size() &&
                s[label_end] != ' ' && s[label_end] != '\t' && s[label_end] != '\n' &&
                s[label_end] != '(' && s[label_end] != ')') {
                label_end++;
            }

            std::string label_str = s.substr(pos, label_end - pos);
            p.label = L2::Label(label_str);

            pos = label_end;  // advance past the label so the function loop starts after it


            while (pos < s.size()) {
                // skip whitespace
                while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n')) pos++;
                if (pos >= s.size() || s[pos] == ')') break;  // hit end of program

                // we should now be at a function's opening '('
                if (s[pos] != '(') break;  

                size_t func_start = pos;
                pos++;
                while (pos < s.size() && s[pos] != ')') pos++;
                if (pos < s.size()) pos++;   //  step past the ')'
                size_t func_end = pos;  // one past the matching ')'

                std::string content = s.substr(func_start, func_end - func_start);

                auto fn = parse_l2_function(content);
                p.functions.push_back(std::move(fn));
            }
            
            return;

            
        }
    };




    template<> struct action <wIncDec> {
        template<typename Input>
        static void apply(const Input& in, Function& f){
            assert(f.getLabel() != "");

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
            instr->setDst(parseW(reg_str));
            instr->setIsInc(is_inc);
            f.instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<wAtWWE> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            Tokenizer tk(in.string());
            std::string dst_str  = tk.next();   // W
            tk.expect("@");
            std::string base_str = tk.next();   // W
            std::string idx_str  = tk.next();   // W
            std::string e_str    = tk.next();   // E

            auto instr = std::make_unique<WWWEInstruction>();
            instr->setDst(parseW(dst_str));
            instr->setBase(parseW(base_str));
            instr->setIdx(parseW(idx_str));
            instr->setScale(std::stoll(e_str));
            f.instructions.push_back(std::move(instr));
        }
    };


     template<> struct action<wStackM> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

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
            f.instructions.push_back(std::move(instr));
        }
    };


    template<> struct action<WaopT> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string aop_str = tk.next();
            std::string t_str   = tk.next();

            auto instr = std::make_unique<ArithInstruction>(InstructionType::WaopT);
            instr->setDst(parseW(w_str));
            instr->setAop(parseAop(aop_str));
            instr->setSrc(parseT(t_str));
            f.instructions.push_back(std::move(instr));
        }
    };


    template<> struct action<WsopN> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
           assert(f.getLabel() != "");

            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string sop_str = tk.next();
            std::string n_str   = tk.next();

            auto instr = std::make_unique<ShiftInstruction>(InstructionType::WsopN);
            instr->setDst(parseW(w_str));
            instr->setSop(parseSop(sop_str));
            instr->setSrc(Number(std::stoll(n_str)));
            f.instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<wIncDecMemory> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
           assert(f.getLabel() != "");

            // form:  W aop mem X M
            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string aop_str = tk.next();
            tk.expect("mem");
            std::string x_str   = tk.next();
            std::string m_str   = tk.next();

            memoryAccess m;
            if (x_str[0] == '%'){
                m.base = Variable(x_str);                        // std::string goes in the variant
            } else {
                m.base = stringToRegister(x_str);      // Register goes in the variant
            }
            m.size = std::stoll(m_str);

            auto instr = std::make_unique<ArithInstruction>(InstructionType::WIncDecMemory);
            instr->setDst(parseW(w_str));
            instr->setAop(parseAop(aop_str));
            instr->setSrc(VALUE(m));
            f.instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<cjump> {
        template<typename Input>
        static void apply(const Input& in, Function& f){
            assert(f.getLabel() != "");

            // form:  cjump t cmp t :label
            Tokenizer tk(in.string());
            tk.expect("cjump");
            std::string left_str  = tk.next();
            std::string cmp_str   = tk.next();
            std::string right_str = tk.next();
            std::string label_tok = tk.next();   // ":name"

            // strip leading ':'
            std::string label_str = (!label_tok.empty() && label_tok[0] == ':')
                                    ? label_tok : label_tok;

            compareStruct cav;
            cav.left  = parseT(left_str);
            cav.cmp   = cmp_str;
            cav.right = parseT(right_str);

            auto instr = std::make_unique<CjumpInstruction>();
            instr->setCmpVal(cav);
            instr->setLabel(Label(label_str));
            f.instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<WsopSx> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            Tokenizer tk(in.string());
            std::string w_str   = tk.next();
            std::string sop_str = tk.next();
            std::string sx_str = tk.next();

            
          

            auto instr = std::make_unique<ShiftInstruction>(InstructionType::WsopSx);
            instr->setDst(parseW(w_str));
            instr->setSop(parseSop(sop_str));
            if (sx_str[0] == '%') instr->setSrc(parseW(sx_str)); // this must be a variable 
            else instr->setSrc(Register::rcx); // else it is always rcx
            f.instructions.push_back(std::move(instr));
        }
    };

    template<> struct action<returnINS> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            auto ret = std::make_unique<ReturnInstruction>();
            f.instructions.push_back(std::move(ret));
        }
    };

    template<> struct action<callInstruction_block> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            // form:  call <callee> <num>   OR   call <builtin> <num>
            Tokenizer tk(in.string());
            tk.expect("call");
            std::string u_str = tk.next();

            auto classify = [](const std::string& s) -> InstructionType {
                if (s.starts_with("@") || s.starts_with("r") || s.starts_with("%")) return InstructionType::CallUN;
                if (s.contains("print"))        return InstructionType::CallPrint;
                if (s.contains("input"))        return InstructionType::CallInput;
                if (s.contains("allocate"))     return InstructionType::CallAllocate;
                if (s.contains("tuple-error"))  return InstructionType::CallTupleError;
                if (s.contains("tensor-error")) return InstructionType::CallTensorError;
                return InstructionType::Unknown;
            };

            auto call = std::make_unique<CallInstruction>(classify(u_str));
            assert(call->type != InstructionType::Unknown);

            if (call->type == InstructionType::CallTensorError){
                std::string n_str = tk.next();
                call->setNum(std::stoll(n_str));
            }

            if (call->type == InstructionType::CallUN) {
                auto parseCallee = [](const std::string& str) -> VALUE {
                    if (str[0] == '@') return Label(str);
                    if (str[0] == '%') return Variable(str);
                    return stringToRegister(str);
                };
                call->setCallee(parseCallee(u_str));
                std::string n_str = tk.next();
                call->setNum(std::stoll(n_str));
            }

            f.instructions.push_back(std::move(call));
        }
    };

    template<> struct action<wFromStackArg> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            Tokenizer tk(in.string());
            std::string w_str = tk.next();
            tk.expect("<-");
            tk.expect("stack-arg");
            std::string m_str = tk.next();

            int64_t stackIdx = std::stoll(m_str);

            // Model stack-arg as: w <- mem rsp <offset>
            // Offset = stackIdx * 8 + (num_locals * 8) + 8
            // (the +8 skips the return address; num_locals*8 skips locals)
            memoryAccess m;
            m.base = Register::rsp;
            m.size = stackIdx * 8 + f.num_locals * 8 + 8;   // changes made to STACK-arg. specifically how it calculates the f.num_locals
            

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignFromStack);
            assign->setTo(parseW(w_str));
            assign->setFrom(VALUE(m));
            f.instructions.push_back(std::move(assign));
        }
    };


    template<> struct action<assignMemoryFromS> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            // form:  mem X M <- S
            Tokenizer tk(in.string());
            tk.expect("mem");
            std::string x_str = tk.next();
            std::string m_str = tk.next();
            tk.expect("<-");
            std::string s_str = tk.next();

            
            memoryAccess m;
            if (x_str[0] == '%'){
                m.base = Variable(x_str);                        // std::string goes in the variant
            } else {
                m.base = stringToRegister(x_str);      // Register goes in the variant
            }
            m.size = std::stoll(m_str);

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignMemoryFromS);
            assign->setTo(VALUE(m));
            assign->setFrom(parseS(s_str));
            f.instructions.push_back(std::move(assign));
        }
    };

    template<> struct action <label_instruction_block> {
        template<typename Input>
        static void apply(const Input& in, Function& f){
            assert(f.getLabel() != "");
            // form:  :name
            Tokenizer tk(in.string());
            std::string tok = tk.next();
            std::string label_name = (!tok.empty() && tok[0] == ':')
                                     ? tok : tok;

            auto lbl = std::make_unique<LabelInstruction>(Label(label_name));
            f.instructions.push_back(std::move(lbl));
        }
    };

    template<> struct action <gotoLabel> {
        template<typename Input>
        static void apply(const Input& in, Function& f){
            assert(f.getLabel() != "");

            // form:  goto :name
            Tokenizer tk(in.string());
            tk.expect("goto");
            std::string tok = tk.next();
            std::string label_name = (!tok.empty() && tok[0] == ':')
                                     ? tok : tok;

            auto lbl = std::make_unique<GotoInstruction>(Label(label_name));
            f.instructions.push_back(std::move(lbl));
        }
    };

    template<> struct action<assignWfromS> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            // form:  W <- S
            Tokenizer tk(in.string());
            std::string w_str = tk.next();
            tk.expect("<-");
            std::string s_str = tk.next();

           

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignFromS);
            assign->setTo(parseW(w_str));
            assign->setFrom(parseS(s_str));
            f.instructions.push_back(std::move(assign));
        }
    };

    template<> struct action<compareAssign> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            // form:  W <- t cmp t
            Tokenizer tk(in.string());
            std::string w_str     = tk.next();
            tk.expect("<-");
            std::string left_str  = tk.next();
            std::string cmp_str   = tk.next();
            std::string right_str = tk.next();

            compareStruct cav;
            cav.left  = parseT(left_str);
            cav.cmp   = cmp_str;
            cav.right = parseT(right_str);

            auto assign = std::make_unique<AssignInstruction>(InstructionType::compareAssign);
            assign->setTo(parseW(w_str));
            assign->setCmpVal(std::move(cav));
            f.instructions.push_back(std::move(assign));
        }
    };

    template<> struct action < assignWfromMemory > {
        template< typename Input >
        static void apply( const Input& in, Function& f){
            assert(f.getLabel() != "");

            // form:  W <- mem X M
            Tokenizer tk(in.string());
            std::string w_str = tk.next();
            tk.expect("<-");
            tk.expect("mem");
            std::string x_str = tk.next();
            std::string m_str = tk.next();

            memoryAccess m;
            if (x_str[0] == '%'){
                m.base = Variable(x_str);                        // std::string goes in the variant
            } else {
                m.base = stringToRegister(x_str);      // Register goes in the variant
            }
            m.size = std::stoll(m_str);

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignFromMemory);
            assign->setTo(parseW(w_str));
            assign->setFrom(VALUE(m));
            f.instructions.push_back(std::move(assign));
        }
    };

    template<> struct action<memoryIncDecT> {
        template<typename Input>
        static void apply(const Input& in, Function& f) {
            assert(f.getLabel() != "");

            // form:  mem X M aop t
            Tokenizer tk(in.string());
            tk.expect("mem");
            std::string x_str   = tk.next();
            std::string m_str   = tk.next();
            std::string aop_str = tk.next();
            std::string t_str   = tk.next();

            auto instr = std::make_unique<MemIncDecInstruction>();

            memoryAccess m;
            if (x_str[0] == '%'){
                m.base = Variable(x_str);                       // std::string goes in the variant
            } else {
                m.base = stringToRegister(x_str);      // Register goes in the variant
            }
            m.size = std::stoll(m_str);

            
            instr->setMem(m);
            instr->setAop(parseAop(aop_str));
            instr->setSrc(parseT(t_str));
            f.instructions.push_back(std::move(instr));
        }
    };



    template<> struct action < l > {
        template< typename Input >
        static void apply( const Input& in, Function& f){
            // form:  @name
            std::string s = in.string();
            size_t at = s.find('@');
            assert(at != std::string::npos);
            std::string str_label = s.substr(at);

            if (f.getLabel() == ""){
                f.setLabel(str_label);
                return;
            }
           
        }
    };


    template<> struct action < functionFormat > {
        template< typename Input >
        static void apply( const Input& in, Function& f){
            
            // form begins with:  <num_args> 
            Tokenizer tk(in.string());
            std::string num_args_str  = tk.next();
            f.setNumArgs(std::stoll(num_args_str));

        }
    };

   


    static bool TRACE = true;

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


    L2::Function parse_l2_function(const std::string& source){
        // 1. Read whole file
        // std::cerr << "[got " << source.size() << " bytes]\n";
        // std::cerr << "---START---\n" << source << "\n---END---\n";
        std::string contents = source;

        // 2. Find the closing ')' of the function
        size_t close = contents.rfind(')');
        if (close == std::string::npos)
            throw std::runtime_error("parse_l2_function: no closing ')'");

        std::string prog_part = contents.substr(0, close + 1);
        std::string tail_part = contents.substr(close + 1);

        // std::cerr << prog_part <<'\n';
        // 3. Parse the function with PEGTL
        L2::Function result;
        pegtl::memory_input<> in(prog_part, "l2_function");
        pegtl::parse<grammar_function, action, my_tracer>(in, result);

        return result;

    }

    L2::Function parse_function_file(const char* fileName){
        // 1. Read whole file
        std::ifstream f(fileName);
        if (!f.is_open()) {
            throw std::runtime_error(std::string("could not open: ") + fileName);
        }
        std::stringstream ss;
        ss << f.rdbuf();
        
        return parse_l2_function(ss.str());
    }

    L2::Program parse_file(const char* fileName){
        // 1. Read whole file
        std::ifstream f(fileName);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string contents = ss.str();
        
        L2::Program result;
        pegtl::memory_input<> in(contents, fileName);
        pegtl::parse<grammar_program, action, my_tracer>(in, result);
        return result;
    }

    
    L2::SpillInput parse_spill_file(const char* fileName) {
        // 1. Read whole file
        std::ifstream f(fileName);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string contents = ss.str();

        // 2. Find the closing ')' of the function
        size_t close = contents.rfind(')');
        if (close == std::string::npos)
            throw std::runtime_error("spill file: no closing ')'");

        std::string prog_part = contents.substr(0, close + 1);
        std::string tail_part = contents.substr(close + 1);

        // std::cerr << prog_part <<'\n';
        // 3. Parse the function with PEGTL
        L2::SpillInput result;
        pegtl::memory_input<> in(prog_part, fileName);
        pegtl::parse<grammar_function, action, my_tracer>(in, result.function);



        // 4. Read the two vars with std::istringstream
        std::istringstream iss(tail_part);
        if (!(iss >> result.target >> result.prefix))
            throw std::runtime_error("spill file: expected <target> <prefix> after function");

        // std::cerr << result.target << result.prefix << '\n';
        return result;
    }
}

