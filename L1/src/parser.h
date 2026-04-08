#include "l1.h"
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp> 


namespace pegtl = TAO_PEGTL_NAMESPACE;


// handle the M case especially using code generation step. for now assume M is number

namespace L1 {

    struct sop : 
        pegtl::sor<
            TAO_PEGTL_STRING("<<="), 
            TAO_PEGTL_STRING("=>>")
        > {};
    
    struct cmp :
        pegtl::sor<
            TAO_PEGTL_STRING("<"),  
            TAO_PEGTL_STRING("<="), 
            TAO_PEGTL_STRING("=")
        > {};

    struct aop :
        pegtl::sor<
            TAO_PEGTL_STRING("+="),  
            TAO_PEGTL_STRING("-="), 
            TAO_PEGTL_STRING("*"), 
            TAO_PEGTL_STRING("&=")
        > {};
    
    struct assign : TAO_PEGTL_STRING("<-"){};
    struct E : pegtl::one<'1', '2', '4', '8'> {};
    struct F : pegtl::one<'1', '3', '4'> {};
    // sx
    struct reg_rcx : TAO_PEGTL_STRING("rcx") {};
    // a
    struct reg_rdi : TAO_PEGTL_STRING("rdi") {};
	struct reg_rsi : TAO_PEGTL_STRING("rsi") {};
    struct reg_rdx : TAO_PEGTL_STRING("rdx") {};
    struct reg_r8 : TAO_PEGTL_STRING("r8") {};
    struct reg_r9 : TAO_PEGTL_STRING("r9") {};
   // W
    struct reg_rax : TAO_PEGTL_STRING("rax") {};
	struct reg_rbx : TAO_PEGTL_STRING("rbx") {};
    struct reg_rbp : TAO_PEGTL_STRING("rbp") {};
    struct reg_r10 : TAO_PEGTL_STRING("r10") {};
    struct reg_r11 : TAO_PEGTL_STRING("r11") {};
    struct reg_r12 : TAO_PEGTL_STRING("r12") {};
    struct reg_r13 : TAO_PEGTL_STRING("r13") {};
    struct reg_r14 : TAO_PEGTL_STRING("r14") {};
    struct reg_r15 : TAO_PEGTL_STRING("r15") {};

   // rsp especial case
   struct reg_rsp : TAO_PEGTL_STRING("rsp") {};

 
    

    struct sx :
        reg_rcx {};

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
		> {
	};

    struct t :
        pegtl::sor <
            X, 
            numberParser
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
		> {
	};
    
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

    
    struct M :
        pegtl::digit {};

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
            TAO_PEGTL_STRING("mem"), 
            X, 
            M
        > {};


    struct instructionParser :
        pegtl::sor<
            pegtl::seq< // w <- s
                W, 
                assign,
                s>, 
            pegtl::seq< // w <- mem x M
                W, 
                assign, 
                memory_access
                >, 
            pegtl::seq< // mem x M <- s
                memory_access, 
                assign, 
                s>, 
            pegtl::seq< // w aop t
                W, 
                aop, 
                t>, 
            pegtl::seq< // w sop sx
                W, 
                sop, 
                sx>,
            pegtl::seq< // w sop N
                W, 
                sop, 
                numberParser>, 
            pegtl::seq< // mem x M += t
                memory_access, 
                TAO_PEGTL_STRING("+="), 
                t>, 
            pegtl::seq< // mem x M -= t 
                memory_access, 
                TAO_PEGTL_STRING("-="), 
                t>,  
            pegtl::seq< // w += mem x M
                W, 
                TAO_PEGTL_STRING("+="), 
                memory_access>,
            pegtl::seq< // w -= mem x M 
                W, 
                TAO_PEGTL_STRING("-="), 
                memory_access>,
            pegtl::seq< // w <- t cmp t
                W, 
                assign, 
                t,
                cmp, 
                t>, 
            pegtl::seq<  // cjump t cmp t label
                TAO_PEGTL_STRING("cjump"),
                t, 
                cmp, 
                t, 
                label>, 
            label, // label instruction
            pegtl::seq< // goto label
                TAO_PEGTL_STRING("goto"), 
                label>, 
            TAO_PEGTL_STRING("return"), // return instruction 
            pegtl::seq<
                TAO_PEGTL_STRING("call"), 
                u, 
                nameParser>
            >
    {};













    // not done consider space 
    struct functionParser : 
        pegtl::seq<
            pegtl::one<'('>,
            l, 
            numberParser,
            numberParser,
            pegtl::plus<instructionParser>,
            pegtl::one<')'>
        > {};

   // not done consider space  
    struct grammar :
		pegtl::must<
			pegtl::one<'('>,
            l,
            pegtl::plus<functionParser>,
            pegtl::one<')'>
		> {};


    template<typename Rule>
    struct action : pegtl::nothing<Rule> {};


    Program parse_file (char* file_name);
}