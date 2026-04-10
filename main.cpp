#include "lib/PEGTL/include/tao/pegtl.hpp"
#include <fstream>
#include "lib/PEGTL/include/tao/pegtl/contrib/analyze.hpp"
#include <iostream>

namespace pegtl = TAO_PEGTL_NAMESPACE;

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



struct l :
    pegtl::seq<
        pegtl::one<'@'>, 
        name> {};

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