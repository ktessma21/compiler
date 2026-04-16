
#include <fstream>
#include "l1.h"
#include <memory>
#include <vector>
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
        spaces, // only a space is allowed between the two numbers
        number, 
        seps_with_comments,
        pegtl::plus<InstructionFormat>
    > {};


struct programORfunction : 
    pegtl::seq<
        spaces, 
        pegtl::sor<
            functionFormat, 
            pegtl::plus<entry_point_rule> // handle many function openings. 
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
    //  std::vector<std::unique_ptr<ASTNode>> items;
    bool new_function = false;


    template<typename Rule>
    struct action : pegtl::normal<Rule> {};


    // struct assignment_block :
    //     pegtl::sor<
    //         compareAssign,      // W <- t cmp t   (before assignWfromS)
    //         assignWfromMemory,  // W <- mem ...   (before assignWfromS, "mem" is specific)
    //         assignWfromS,       // W <- S         (generic <- fallback)
    //         assignMemoryFromS> {};

    // InstructionType currentInstructionType = InstructionType::Unknown;


    template<> struct action <wIncDec> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = 0;

            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;

            size_t reg_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t' && (s[pos] != '+') && (s[pos] != '-')) pos++;
            std::string reg_str = s.substr(reg_start, pos - reg_start);

            VALUE reg = stringToRegister(reg_str);
            auto instr = std::make_unique<IncDecInstruction>();
            instr->setDst(reg);

            if (s.substr(pos, 2) == "++"){
                instr->setIsInc(true);
            } else if (s.substr(pos, 2) == "--"){
                instr->setIsInc(false);
            }else{
                throw std::runtime_error("invalid increment/decrement instruction: " + s);
            }
            p.functions.back().instructions.push_back(std::move(instr));

        }
    };

    template<> struct action<wAtWWE> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = 0;

            // skip spaces
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            // read dst W
            size_t dst_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string dst_str = s.substr(dst_start, pos - dst_start);

            // skip spaces, skip @
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            assert(s[pos] == '@');
            pos++;

            // skip spaces
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            // read base W
            size_t base_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string base_str = s.substr(base_start, pos - base_start);

            // skip spaces
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            // read idx W
            size_t idx_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string idx_str = s.substr(idx_start, pos - idx_start);

            // skip spaces
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            // read E — rest of string trimmed
            std::string e_str = s.substr(pos);
            while (!e_str.empty() && (e_str.back()==' '||e_str.back()=='\t')) e_str.pop_back();

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

                    std::string s = in.string();
                    size_t pos = 0;

                    // read W
                    while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
                    size_t w_start = pos;
                    while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
                    std::string w_str = s.substr(w_start, pos - w_start);

                    // read aop
                    while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
                    size_t aop_start = pos;
                    while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
                    std::string aop_str = s.substr(aop_start, pos - aop_start);

                    // read t
                    while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
                    std::string t_str = s.substr(pos);
                    while (!t_str.empty() && (t_str.back()==' '||t_str.back()=='\t')) t_str.pop_back();

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

            std::string s = in.string();
            size_t pos = 0;

            // skip spaces, read W
            while (pos < s.size() && (s[pos]=='\]]] '||s[pos]=='\t')) pos++;
            size_t w_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string w_str = s.substr(w_start, pos - w_start);

            // skip spaces, read sop
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t sop_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string sop_str = s.substr(sop_start, pos - sop_start);

            // skip spaces, read number
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            std::string n_str = s.substr(pos);
            while (!n_str.empty() && (n_str.back()==' '||n_str.back()=='\t')) n_str.pop_back();

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

            std::string s = in.string();
            size_t pos = 0;

            // read W
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t w_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string w_str = s.substr(w_start, pos - w_start);

            // read aop (+=  or -=)
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t aop_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string aop_str = s.substr(aop_start, pos - aop_start);

            // find mem
            size_t mem_pos = s.find("mem", pos);
            assert(mem_pos != std::string::npos);
            pos = mem_pos + 3;

            // read X
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t x_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string x_str = s.substr(x_start, pos - x_start);

            // read M
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            std::string m_str = s.substr(pos);
            while (!m_str.empty() && (m_str.back()==' '||m_str.back()=='\t')) m_str.pop_back();

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

            std::string s = in.string();
            size_t pos = s.find("cjump") + 5;

            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t left_end = s.find_first_of(" ", pos);
            std::string left_str = s.substr(pos, left_end - pos);
            pos = left_end;
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t cmp_end = s.find_first_of(" ", pos);
            std::string cmp_str = s.substr(pos, cmp_end - pos);
            pos = cmp_end;
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t right_end = s.find_first_of(" ", pos);
            std::string right_str = s.substr(pos, right_end - pos);
            pos = right_end;
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            pos += 1; // skip ':'
            std::size_t label_end = s.find_first_of(" ", pos);
            std::string label_str = s.substr(pos, label_end - pos);




            compareStruct cav;

            cav.left = std::make_unique<VALUE>(parseT(left_str));
            cav.cmp = cmp_str;
            cav.right = std::make_unique<VALUE>(parseT(right_str));
            
            auto instr = std::make_unique<CjumpInstruction>();  // no &
            instr->setCmpVal(cav);
            instr->setLabel(Label(label_str));
            p.functions.back().instructions.push_back(std::move(instr));
            
            // std::cerr << "parsed cjump with left\n";

        }
    };

    template<> struct action<WsopSx> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = 0;

            // read W
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t w_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string w_str = s.substr(w_start, pos - w_start);

            // read sop
            while (pos < s.size() && (s[pos]==' '||s[pos]=='\t')) pos++;
            size_t sop_start = pos;
            while (pos < s.size() && s[pos]!=' ' && s[pos]!='\t') pos++;
            std::string sop_str = s.substr(sop_start, pos - sop_start);

            // sx is always rcx
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

            std::unique_ptr<ReturnInstruction> ret = std::make_unique<ReturnInstruction>();
            p.functions.back().instructions.push_back(std::move(ret));
        }
    };
    
    template<> struct action<callInstruction_block> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());
            // std::cerr << "parsing call instruction : " << in.string() << '\n';
             
            std::string s = in.string();
            size_t pos = s.find_first_of("call") + 4;

                // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
                // read u value
            size_t u_end = s.find_first_of(" ", pos);
            std::string u_str = s.substr(pos, u_end - pos);

            // std::cerr<< "parsing call instruction with u_str: " << u_str << '\n';

            auto parser = [](const std::string& s) -> InstructionType {
                if (s.starts_with("@") || s.starts_with("r")) return InstructionType::CallUN;
                if (s.contains("print")) return InstructionType::CallPrint;
                if (s.contains("input")) return InstructionType::CallInput;
                if (s.contains("allocate")) return InstructionType::CallAllocate;
                if (s.contains("tuple-error")) return InstructionType::CallTupleError;
                if (s.contains("tensor-error")) return InstructionType::CallTensorError;
                return InstructionType::Unknown;
            };

            std::unique_ptr<CallInstruction> call = std::make_unique<CallInstruction>(parser(u_str));

            assert(call->type != InstructionType::Unknown);
            if (call->type == InstructionType::CallUN){
                    // parse S — label, register, or number
                auto parseS = [](const std::string& str) -> VALUE {
                    if (str[0] == '@') return Label(str.substr(1));
                    return stringToRegister(str);
                };

                call -> setCallee(parseS(u_str));
                pos = u_end;
                // skip spaces
                while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

// read u value
                size_t digit = pos;
                while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
                std::string digit_str = s.substr(digit, pos - digit);   
                call -> setNum(std::stoll(digit_str));
            }
            
           

            p.functions.back().instructions.push_back(std::move(call));
        }
    };
   


    template<> struct action<assignMemoryFromS> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = 0;

            // skip leading spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // find and skip past "mem"
            size_t mem_pos = s.find("mem", pos);
            assert(mem_pos != std::string::npos);
            pos = mem_pos + 3;

            // skip spaces after "mem"
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read X register
            size_t x_start = pos;
            while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
            std::string x_str = s.substr(x_start, pos - x_start);

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read M number
            size_t m_start = pos;
            while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
            std::string m_str = s.substr(m_start, pos - m_start);

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // skip "<-"
            pos += 2;

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read S — rest of string trimmed
            std::string s_str = s.substr(pos);
            while (!s_str.empty() && (s_str.back() == ' ' || s_str.back() == '\t')) {
                s_str.pop_back();
            }

            // parse S — label, register, or number
            auto parseS = [](const std::string& str) -> VALUE {
                if (str[0] == '@') return Label(str.substr(1));
                if (str[0] == ':') return Label(str.substr(1));
                try {
                    return stringToRegister(str);
                } catch (...) {
                    return Number(std::stoll(str));
                }
            };

            // build memory access for destination
            memoryAccess m;
            m.x_value = stringToRegister(x_str);
            m.size    = std::stoll(m_str);

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignMemoryFromS);
            assign->setTo(VALUE(m));          // mem X M is destination
            assign->setFrom(parseS(s_str));   // S is source

            p.functions.back().instructions.push_back(std::move(assign));
        }
    };

    template<> struct action <label_instruction_block> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
            // std::cerr << "parsing label instruction\n";
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = s.find(':');
            pos += 1;
            assert(pos != std::string::npos);
            
            size_t next = s.find(' ', pos);
            std::string label_name = s.substr(pos, next - pos);
            // std::cerr << "parsed label: " << label_name << "\n";

            auto label = std::make_unique<LabelInstruction>(label_name);
            p.functions.back().instructions.push_back(std::move(label));

        }
    };

    template<> struct action <gotoLabel> {
        template<typename Input>
        static void apply(const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = s.find(':');
            pos += 1;
            assert(pos != std::string::npos);
            
            size_t next = s.find(' ', pos);
            std::string label_name = s.substr(pos, next - pos);

            auto label = std::make_unique<GotoInstruction>(label_name);
            p.functions.back().instructions.push_back(std::move(label));

        }
    };

    template<> struct action<assignWfromS> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = 0;

            // skip leading spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read destination W
            size_t w_start = pos;
            while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
            std::string w_str = s.substr(w_start, pos - w_start);

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // skip "<-"
            pos += 2;

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read S — rest of string trimmed
            std::string s_str = s.substr(pos);
            while (!s_str.empty() && (s_str.back() == ' ' || s_str.back() == '\t')) {
                s_str.pop_back();
            }

            // parse S — could be label (@foo), label (:foo), register, or number
            auto parseS = [](const std::string& str) -> VALUE {
                if (str[0] == '@') return Label(str);       // l
                if (str[0] == ':') return Label(str);       // label
                try {
                    return stringToRegister(str);           // register
                } catch (...) {
                    return Number(std::stoll(str));         // number
                }
            };

            auto assign = std::make_unique<AssignInstruction>(InstructionType::AssignFromS);
            assign->setTo(stringToRegister(w_str));
            assign->setFrom(parseS(s_str));

            p.functions.back().instructions.push_back(std::move(assign));
        }
    };

    template<> struct action<compareAssign> {
        template<typename Input>
        static void apply(const Input& in, Program& p) {
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::string s = in.string();
            size_t pos = 0;

            // skip leading spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read destination W
            size_t w_start = pos;
            while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
            std::string w_str = s.substr(w_start, pos - w_start);

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // skip "<-"
            pos += 2;

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read left t operand
            size_t left_start = pos;
            while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
            std::string left_str = s.substr(left_start, pos - left_start);

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read cmp operator (<=, <, =)
            std::string cmp_op;
            if (pos + 1 < s.size() && s.substr(pos, 2) == "<=") {
                cmp_op = "<=";
                pos += 2;
            } else if (s[pos] == '<') {
                cmp_op = "<";
                pos += 1;
            } else if (s[pos] == '=') {
                cmp_op = "=";
                pos += 1;
            }

            // skip spaces
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read right t operand — rest of string trimmed
            std::string right_str = s.substr(pos);
            while (!right_str.empty() && (right_str.back() == ' ' || right_str.back() == '\t')) {
                right_str.pop_back();
            }

            // parse a t value — either register or number
            auto parseT = [](const std::string& str) -> VALUE {
                try {
                    return stringToRegister(str);
                } catch (...) {
                    return Number(std::stoll(str));
                }
            };

            // build compareStruct
            compareStruct cav;
            cav.left  = std::make_unique<VALUE>(parseT(left_str));
            cav.cmp   = cmp_op;
            cav.right = std::make_unique<VALUE>(parseT(right_str));

            // build instruction
            auto assign = std::make_unique<AssignInstruction>(InstructionType::compareAssign);
            assign->setTo(stringToRegister(w_str));
            assign -> setCmpVal(std::move(cav));

            p.functions.back().instructions.push_back(std::move(assign));
        }
    };

    template<> struct action < assignWfromMemory > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());

            std::unique_ptr<AssignInstruction> assign = std::make_unique<AssignInstruction>(InstructionType::AssignFromMemory);
            
            std::string s = in.string();


            size_t pos = 0;
            // skip any leading spaces.
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

            // read register — everything until next space
            size_t reg_start = pos;
            while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
            std::string reg_str = s.substr(reg_start, pos - reg_start);

            VALUE v = stringToRegister(reg_str);
            assign -> setTo(v);

            size_t mem_pos = s.find("mem", pos);
            pos = mem_pos + 3;  // skip past "mem"
            assert(mem_pos != std::string::npos);


            // skip spaces after "mem"
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
            
            // read register — everything until next space
            reg_start = pos;
            while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
            reg_str = s.substr(reg_start, pos - reg_start);
            
            // skip spaces between register and number
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
            
            // read number — everything remaining
            std::string num_str = s.substr(pos);

            // build the memoryAccess
            memoryAccess m;
            m.x_value = stringToRegister(reg_str);
            m.size    = std::stoll(num_str);

            VALUE mem_value = m;
            assign -> setFrom(mem_value);

            p.functions.back().instructions.push_back(std::move(assign));
        
        }

    };

   

    

    template<> struct action < l > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            size_t pos = in.string().find('@');
            pos += 1;
            assert(pos != std::string::npos);
            std::string str_label = in.string().substr(pos);
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
            std::string s = in.string();
            size_t pos = 0;
            // skip leading spaces 
            while (pos < s.size() && (s[pos] == ' '|| s[pos] == '\t')) pos++;

            size_t num_args_start = in.string().find(' ', pos);
            std::string num_args_str = s.substr(pos, num_args_start - pos);
            pos = num_args_start + 1;
            while (pos < s.size() && (s[pos] == ' '|| s[pos] == '\t')) pos++;
            size_t num_args_local = in.string().find(' ', pos);
            std::string num_local_str= s.substr(pos, num_args_local - pos);

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
            // current_function = &p.functions.back(); dangerous idea because when the push_back needs more space it will reallocate the entire to another address so the pointer will be dangling. 
            return;
        }
    };
    
    // template<> struct action < grammar > {
    //     template< typename Input >
    //     static void apply( const Input& in, Program& p){
    //         std::cout << in.string() << std::endl;
    //     }
    // };

    // fetch the last function

    Program parse_file (char *fileName){
    
    FILE *file = fopen(fileName, "r");

    if (!file){
        std::cerr <<fileName << " : file not found." << std::endl;
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

    // Only matching one function. 
    file_input< > fileInput(fileName);
    Program p;
    parse< grammar, action >(fileInput, p);

    // std::cout << p.to_string() << std::endl ;

    return p;
  }
}

