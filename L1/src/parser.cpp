
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

struct callInstruction :
    pegtl::seq<
        pstring("call"),
        spaces,
        pegtl::sor<
            callPrint,        // call print 1        (specific string)
            callInput,        // call input 0        (specific string)
            callAllocate,     // call allocate 2     (specific string)
            calltupleError,   // call tuple-error 3  (specific string)
            calltensorError,  // call tensor-error F (specific string)
            callUN            // call u number       (generic fallback)
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
            callInstruction,
            cjump,              // cjump t cmp t label
            label,                // just label
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

    


    /* FUll ITEM collecting */
    //  std::vector<std::unique_ptr<ASTNode>> items;
    bool value_is_memory_access = false;


    template<typename Rule>
    struct action : pegtl::normal<Rule> {};


    // struct assignment_block :
    //     pegtl::sor<
    //         compareAssign,      // W <- t cmp t   (before assignWfromS)
    //         assignWfromMemory,  // W <- mem ...   (before assignWfromS, "mem" is specific)
    //         assignWfromS,       // W <- S         (generic <- fallback)
    //         assignMemoryFromS> {};

    // InstructionType currentInstructionType = InstructionType::Unknown;

    template<> struct action < label > {
            template< typename Input >
            static void start( const Input& in, Program& p){
                assert(!p.label.empty());
                assert(!p.functions.empty());
                assert(!p.functions.back().instructions.empty());

                if (isAssignType(p.functions.back().instructions.back() -> type)){
                    auto* assign = dynamic_cast<AssignInstruction*>(
                        p.functions.back().instructions.back().get()
                    );
                    VALUE v = in.string();
                    assert(!assign -> hasTo());
                    assign -> setTo(v);
                    return;
                }
                
                
                // assert(isAssignType(assign -> getType()));
            }

        };

    template<> struct action < memory_access_block > {
            template< typename Input >
            static void start( const Input& in, Program& p){
                value_is_memory_access = true;
            }

            template< typename Input >
            static void apply( const Input& in, Program& p){
                        
                std::string s = in.string();
                
                // skip "mem" and any surrounding spaces
                size_t pos = s.find("mem");
                pos += 3;  // skip past "mem"
                
                // skip spaces after "mem"
                while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
                
                // read register — everything until next space
                size_t reg_start = pos;
                while (pos < s.size() && s[pos] != ' ' && s[pos] != '\t') pos++;
                std::string reg_str = s.substr(reg_start, pos - reg_start);
                
                // skip spaces between register and number
                while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
                
                // read number — everything remaining
                std::string num_str = s.substr(pos);

                // build the memoryAccess
                memoryAccess m;
                m.x_value = stringToRegister(reg_str);
                m.size    = std::stoll(num_str);
                
                std::cerr << "mem: reg=" << reg_str << " size=" << num_str << "\n";  // debug
                auto* assign = dynamic_cast<AssignInstruction*>(
                    p.functions.back().instructions.back().get()
                );
                // assume we are in assignment 
                if (value_is_memory_access && isAssignType(assign -> type)){
                    VALUE v = m;
                    if (!assign -> hasFrom()){
                        assign -> setFrom(v);
                        return;
                    }
                    if (!assign -> hasTo()){
                        assign -> setTo(v);
                        return;
                    }
                    std::cerr << "weirdly both are assigned without waiting me";
                    exit(1);
                }

                value_is_memory_access = false;
            }

            template< typename Input >
            static void failure( const Input& in, Program& p){
                value_is_memory_access = false;
            }

        };


    template<> struct action < W > {
            template< typename Input >
            static void apply( const Input& in, Program& p){
                assert(!p.label.empty());
                assert(!p.functions.empty());
                assert(!p.functions.back().instructions.empty());

                auto* assign = dynamic_cast<AssignInstruction*>(
                    p.functions.back().instructions.back().get()
                );
                // assume we are in assignment 
                if (!value_is_memory_access && isAssignType(assign -> type)){
                    VALUE v = stringToRegister(in.string());
                    if (!assign -> hasFrom()){
                        assign -> setFrom(v);
                        return;
                    }
                    if (!assign -> hasTo()){
                        assign -> setTo(v);
                        return;
                    }
                    std::cerr << "weirdly both are assigned without waiting me";
                    exit(1);
                }
            }
        };


    template<> struct action < assignMemoryFromS > {
            template< typename Input >
            static void start( const Input& in, Program& p){
                assert(!p.label.empty());
                assert(!p.functions.empty());
                assert(!p.functions.back().instructions.empty());
                auto* assign = dynamic_cast<AssignInstruction*>(
                    p.functions.back().instructions.back().get()
                );
                assign -> setType(InstructionType::AssignMemoryFromS);
            }

        };


    template<> struct action < assignWfromS > {
        template< typename Input >
        static void start( const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());
            assert(!p.functions.back().instructions.empty());
            auto* assign = dynamic_cast<AssignInstruction*>(
                p.functions.back().instructions.back().get()
            );
            assign -> setType(InstructionType::AssignFromS);
        }
    };

    template<> struct action < compareAssign > {
        template< typename Input >
        static void start( const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());
            assert(!p.functions.back().instructions.empty());
            auto* assign = dynamic_cast<AssignInstruction*>(
                p.functions.back().instructions.back().get()
            );
            assign -> setType(InstructionType::CompareAssign);
        }
    };

    template<> struct action < assignWfromMemory > {
        template< typename Input >
        static void start( const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());
            assert(!p.functions.back().instructions.empty());
            auto* assign = dynamic_cast<AssignInstruction*>(
                p.functions.back().instructions.back().get()
            );
            assign -> setType(InstructionType::AssignFromMemory);
        }

    };

    template<> struct action < assignment_block > {
        template< typename Input >
        static void start( const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());
            assert(!p.functions.back().instructions.empty());
            p.functions.back().instructions.push_back(
                std::make_unique<AssignInstruction>()
            );
        }
        template< typename Input >
        static void failure( const Input& in, Program& p){
            assert(!p.label.empty());
            assert(!p.functions.empty());
            assert(!p.functions.back().instructions.empty());
            p.functions.back().instructions.pop_back();
        }

    };

    

    template<> struct action < l > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            if (p.label.empty()){
                p.label = in.string();
                p.functions.push_back(Function()); // there must be at least one function 
                return;
            }
            if (p.functions.back().getLabel().empty()){
                p.functions.back().setLabel(in.string());
            }
        }
    };

    template<> struct action < number > {
        template< typename Input >
        static void apply( const Input& in, Program& p){ 
            if (p.label.empty() || p.functions.empty()){
                return;  // this is just a program
            }

            // if (p.bac)

            // check if it is called from function point of view 
            if (!p.functions.back().args_set){
                p.functions.back().setNumArgs(std::stoll(in.string()));
            }
            if (!p.functions.back().local_set){
                p.functions.back().setNumLocals(std::stoll(in.string()));
            }

            // check if it is called from instruction point of view 
        }
    };

    template<> struct action < functionFormat > {
        template< typename Input >
        static void apply( const Input& in, Program& p){
            // auto label = std::move(items.back().get());
            // items.pop_back();

            

            // std::unique_ptr<Function> function = std::make_unique<Function>();
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

    std::cout << p.to_string() << std::endl ;

    return p;
  }
}



 // bool TRACE = false; // toggle this

    // template< typename Rule >
    // struct my_tracer : pegtl::normal< Rule > {
    //     template< typename Input, typename... States >
    //     static void start( const Input& in, States&&... ) {
    //         if (!TRACE) return;
    //         std::cerr << "try   " << pegtl::demangle< Rule >() 
    //                 << " at line " << in.position().line 
    //                 << " col " << in.position().column << "\n";
    //     }

    //     template< typename Input, typename... States >
    //     static void success( const Input& in, States&&... ) {
    //         if (!TRACE) return;
    //         std::cerr << "ok    " << pegtl::demangle< Rule >() << "\n";
    //     }

    //     template< typename Input, typename... States >
    //     static void failure( const Input& in, States&&... ) {
    //         if (!TRACE) return;
    //         std::cerr << "FAIL  " << pegtl::demangle< Rule >() << "\n";
    //     }
    // };
