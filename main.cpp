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

struct l :
    pegtl::seq<
        pegtl::one<'@'>, 
        name> {};

struct returnINS : TAO_PEGTL_STRING("return"){};
        

struct functionFormat :
    pegtl::seq<
        number, 
        spaces, // only a space is allowed between the twow
        number, 
        seps, 
        spaces,
        returnINS
    > {};




  
struct programORfunction : 
    pegtl::seq<
        spaces, 
        pegtl::sor<
            functionFormat, 
            pegtl::plus<entry_point_rule> // handle many function openings. 
        >
    >{};


 struct entry_point_rule:
    pegtl::seq<
      seps_with_comments,
      pegtl::seq<spaces, pegtl::one< '(' >>,
      l,
      seps_with_comments,
      programORfunction,      // it used to be a program, so now wait for function from now on. or it's function wait for number
      seps_with_comments,
      pegtl::seq<spaces, pegtl::one< ')' >>,
      spaces
    > { };

  struct grammar : 
    pegtl::must< 
      entry_point_rule
    > {};




template< typename Rule >
struct action : pegtl::nothing< Rule > {};


template<> struct action < functionFormat> {
    template< typename Input >
    static void apply( const Input& in){
        std::cout << in.string() << std::endl;
    }
};

template< typename Rule >
struct my_tracer : pegtl::normal< Rule > {
    template< typename Input, typename... States >
    static void start( const Input& in, States&&... ) {
        std::cerr << "try   " << pegtl::demangle< Rule >() 
                  << " at line " << in.position().line 
                  << " col " << in.position().column << "\n";
    }

    template< typename Input, typename... States >
    static void success( const Input& in, States&&... ) {
        std::cerr << "ok    " << pegtl::demangle< Rule >() << "\n";
    }

    template< typename Input, typename... States >
    static void failure( const Input& in, States&&... ) {
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