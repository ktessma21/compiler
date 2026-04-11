#include "lib/PEGTL/include/tao/pegtl.hpp"
#include <fstream>
#include "lib/PEGTL/include/tao/pegtl/contrib/analyze.hpp"
#include <iostream>

namespace pegtl = TAO_PEGTL_NAMESPACE;

#define pstring TAO_PEGTL_STRING
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

struct M : pegtl::digit {};


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
// struct WaopT:
//     pegtl::seq<
//             W, 
//             spaces,
//             aop,
//             spaces, 
//             t
//         >{};
// Clean function format 
struct returnINS : pstring("return"){};

struct Instruction : 
        pegtl::sor<
            compareAssign,      // W <- t cmp t   (before assignWfromS)
            assignWfromMemory,  // W <- mem ...   (before assignWfromS, "mem" is specific)
            assignWfromS,       // W <- S         (generic <- fallback)
            wIncDecMemory,      // W op= mem ...  (before WaopT, "mem" is specific)
            WsopSx,             // W sop sx       (before WsopN, sx is specific)
            WsopN,              // W sop number   (sop fallback)
            WaopT,              // W aop t        (generic aop)
            memoryIncDecT,      // mem X M op= t  (unique prefix)
            returnINS           // return         (unique)
        >{};





// Don't touch from now on. Extremely stable parsing code. 
struct InstructionFormat : 
    pegtl::seq<
        spaces,
        Instruction,
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




template< typename Rule >
struct action : pegtl::nothing< Rule > {};


bool TRACE = true; // toggle this

template< typename Rule >
struct my_tracer : pegtl::normal< Rule > {
    template< typename Input, typename... States >
    static void start( const Input& in, States&&... ) {
        if (!TRACE) return;
        std::cerr << "try   " << pegtl::demangle< Rule >() 
                  << " at line " << in.position().line 
                  << " col " << in.position().column << "\n";
    }

    template< typename Input, typename... States >
    static void success( const Input& in, States&&... ) {
        if (!TRACE) return;
        std::cerr << "ok    " << pegtl::demangle< Rule >() << "\n";
    }

    template< typename Input, typename... States >
    static void failure( const Input& in, States&&... ) {
        if (!TRACE) return;
        std::cerr << "FAIL  " << pegtl::demangle< Rule >() << "\n";
    }
};

// template<> struct action < content > {
//     template< typename Input >
//     static void apply( const Input& in){
//         std::cout << in.string() << std::endl;
//     }
// };

template<> struct action < grammar > {
    template< typename Input >
    static void apply( const Input& in){
        std::cout << in.string() << std::endl;
    }
};

extern "C" int64_t go() {
    if (pegtl::analyze< grammar >() != 0) {
        std::cerr << "There are problems with the grammar" << std::endl;
        return 1;
    }

    std::string fileName = "test1.txt";
    std::fstream file;
    file.open(fileName, std::ios::in);
    if (!file.is_open()){
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }

    try {
        pegtl::file_input< > fileInput(fileName);
        pegtl::parse< grammar, action, my_tracer>(fileInput);
    } catch( const pegtl::parse_error& e ) {
        const auto p = e.position_object();
        std::cerr << "Parse error at line " << p.line 
                  << ", col " << p.column << std::endl;
        std::cerr << e.what() << std::endl;

    }
    return 0;
}