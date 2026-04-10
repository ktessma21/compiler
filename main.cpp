#include "lib/PEGTL/include/tao/pegtl.hpp"
#include <fstream>
#include "lib/PEGTL/include/tao/pegtl/contrib/analyze.hpp"
#include <iostream>

namespace pegtl = TAO_PEGTL_NAMESPACE;

    struct comment: 
        pegtl::disable< 
        TAO_PEGTL_STRING( "//" ), 
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

    struct sop : 
        pegtl::sor<
            TAO_PEGTL_STRING("<<="), 
            TAO_PEGTL_STRING("=>>")
        > {};
    
    struct cmp :
        pegtl::sor<
            TAO_PEGTL_STRING("<="), 
            TAO_PEGTL_STRING("<"),  
            TAO_PEGTL_STRING("=")
        > {};

    struct aop :
        pegtl::sor<
            TAO_PEGTL_STRING("+="),  
            TAO_PEGTL_STRING("-="), 
            TAO_PEGTL_STRING("*="), 
            TAO_PEGTL_STRING("&=")
        > {};

    struct assign : TAO_PEGTL_STRING("<-"){};
    struct E : pegtl::one<'1', '2', '4', '8'> {};
    struct F : pegtl::one<'1', '3', '4'> {};

    struct reg_rcx : TAO_PEGTL_STRING("rcx") {};
    struct reg_rdi : TAO_PEGTL_STRING("rdi") {};
    struct reg_rsi : TAO_PEGTL_STRING("rsi") {};
    struct reg_rdx : TAO_PEGTL_STRING("rdx") {};
    struct reg_r8  : TAO_PEGTL_STRING("r8")  {};
    struct reg_r9  : TAO_PEGTL_STRING("r9")  {};
    struct reg_rax : TAO_PEGTL_STRING("rax") {};
    struct reg_rbx : TAO_PEGTL_STRING("rbx") {};
    struct reg_rbp : TAO_PEGTL_STRING("rbp") {};
    struct reg_r10 : TAO_PEGTL_STRING("r10") {};
    struct reg_r11 : TAO_PEGTL_STRING("r11") {};
    struct reg_r12 : TAO_PEGTL_STRING("r12") {};
    struct reg_r13 : TAO_PEGTL_STRING("r13") {};
    struct reg_r14 : TAO_PEGTL_STRING("r14") {};
    struct reg_r15 : TAO_PEGTL_STRING("r15") {};
    struct reg_rsp : TAO_PEGTL_STRING("rsp") {};

    struct sx : reg_rcx {};

    struct a :
        pegtl::sor<
            sx,
            reg_rdi, 
            reg_rsi, 
            reg_rdx,
            reg_r8, 
            reg_r9
        > {};

    struct W :
        pegtl::sor<
            a, 
            reg_rax, 
            reg_rbx, 
            reg_rbp, 
            reg_r10, 
            reg_r11, 
            reg_r12, 
            reg_r13, 
            reg_r14,
            reg_r15
        > {};

    struct X :
        pegtl::sor<
            W, 
            reg_rsp
        > {};

    struct nameParser :
        pegtl::seq<
            pegtl::sor<
                pegtl::alpha,
                pegtl::one< '_' >
            >,
            pegtl::star<
                pegtl::sor<
                    pegtl::alpha,
                    pegtl::one< '_' >,
                    pegtl::digit
                >
            >
        > {};

    struct label :
        pegtl::seq<
            pegtl::one<':'>, 
            nameParser
        > {};
    
    struct l :
        pegtl::seq<
            pegtl::one<'@'>,
            nameParser
        > {};

    struct numberParser :
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
        > {};

    struct t :
        pegtl::sor<
            X, 
            numberParser
        > {};

    struct s :
        pegtl::sor<
            t, 
            label, 
            l
        > {};

    struct u :
        pegtl::sor<
            W, 
            l
        > {};

    struct memory_access :
        pegtl::seq<
            spaces,
            TAO_PEGTL_STRING("mem"), 
            spaces,
            X, 
            spaces,
            numberParser
        > {};

struct instructionParser :
    pegtl::sor<
        // w <- mem x M
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, assign, spaces, TAO_PEGTL_STRING("mem")>>,
            spaces, W, spaces, assign, spaces, memory_access>,

        // w <- t cmp t
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, assign, spaces, t, spaces, cmp>>,
            spaces, W, spaces, assign, spaces, t, spaces, cmp, spaces, t>,

        // w <- s
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, assign>>,
            spaces, W, spaces, assign, spaces, s>,

        // mem x M += t
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("mem"), spaces, X, spaces, numberParser, spaces, TAO_PEGTL_STRING("+=")>>,
            spaces, memory_access, spaces, TAO_PEGTL_STRING("+="), spaces, t>,

        // mem x M -= t
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("mem"), spaces, X, spaces, numberParser, spaces, TAO_PEGTL_STRING("-=")>>,
            spaces, memory_access, spaces, TAO_PEGTL_STRING("-="), spaces, t>,

        // mem x M <- s
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("mem"), spaces, X, spaces, numberParser, spaces, assign>>,
            spaces, memory_access, spaces, assign, spaces, s>,

        // w += mem x M
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, TAO_PEGTL_STRING("+="), spaces, TAO_PEGTL_STRING("mem")>>,
            spaces, W, spaces, TAO_PEGTL_STRING("+="), spaces, memory_access>,

        // w -= mem x M
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, TAO_PEGTL_STRING("-="), spaces, TAO_PEGTL_STRING("mem")>>,
            spaces, W, spaces, TAO_PEGTL_STRING("-="), spaces, memory_access>,

        // w aop t
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, aop>>,
            spaces, W, spaces, aop, spaces, t>,

        // w sop sx
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, sop, spaces, sx>>,
            spaces, W, spaces, sop, spaces, sx>,

        // w sop N
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, sop>>,
            spaces, W, spaces, sop, spaces, numberParser>,

        // w ++
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, TAO_PEGTL_STRING("++")>>,
            spaces, W, spaces, TAO_PEGTL_STRING("++")>,

        // w --
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, TAO_PEGTL_STRING("--")>>,
            spaces, W, spaces, TAO_PEGTL_STRING("--")>,

        // W @ W W E  (lea)
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, W, spaces, pegtl::one<'@'>>>,
            spaces, W, spaces, pegtl::one<'@'>, spaces, W, spaces, W, spaces, E>,

        // call print 1
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("print")>>,
            spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("print"), spaces, pegtl::one<'1'>>,

        // call input 0
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("input")>>,
            spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("input"), spaces, pegtl::one<'0'>>,

        // call allocate 2
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("allocate")>>,
            spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("allocate"), spaces, pegtl::one<'2'>>,

        // call tuple-error 3
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("tuple-error")>>,
            spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("tuple-error"), spaces, pegtl::one<'3'>>,

        // call tensor-error F
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("tensor-error")>>,
            spaces, TAO_PEGTL_STRING("call"), spaces, TAO_PEGTL_STRING("tensor-error"), spaces, F>,

        // call u N
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("call")>>,
            spaces, TAO_PEGTL_STRING("call"), spaces, u, spaces, numberParser>,

        // cjump
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("cjump")>>,
            spaces, TAO_PEGTL_STRING("cjump"), spaces, t, spaces, cmp, spaces, t, spaces, label>,

        // goto
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("goto")>>,
            spaces, TAO_PEGTL_STRING("goto"), spaces, label>,

        // return
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, TAO_PEGTL_STRING("return")>>,
            spaces, TAO_PEGTL_STRING("return")>,

        // comment line
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, comment>>,
            spaces, comment>,

        // label
        pegtl::seq<
            pegtl::at<pegtl::seq<spaces, label>>,
            spaces, label>
    >{};

struct Instructions_rule :
    pegtl::plus<
        pegtl::seq<
            seps_with_comments,
            pegtl::bol,
            instructionParser
        >
    > {};

   struct functionParser : 
    pegtl::seq<
        pegtl::one<'('>,
        l,
        seps_with_comments,
        numberParser,
        seps_with_comments,
        numberParser,
        seps_with_comments,
        Instructions_rule,
        seps_with_comments,
        pegtl::one<')'>
    > {};


struct functions :
    pegtl::plus<
        pegtl::seq<
            seps_with_comments,
            functionParser
        >
    > {};


 struct entry_point_rule:
    pegtl::seq<
      seps_with_comments,
      pegtl::seq<spaces, pegtl::one< '(' >>,
      seps_with_comments,
      l,
      seps_with_comments,
      functions,
      seps_with_comments,
      pegtl::seq<spaces, pegtl::one< ')' >>,
      seps
    > { };

  struct grammar : 
    pegtl::must< 
      entry_point_rule
    > {};

template< typename Rule >
struct action : pegtl::nothing< Rule > {};


template<> struct action < functionParser > {
    template< typename Input >
    static void apply( const Input& in){
        std::cout << in.string() << std::endl;
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
        pegtl::parse< grammar, action >(fileInput);
    } catch( const pegtl::parse_error& e ) {
        const auto p = e.position_object();
        std::cerr << "Parse error at line " << p.line 
                  << ", col " << p.column << std::endl;
        std::cerr << e.what() << std::endl;
    }
    return 0;
}