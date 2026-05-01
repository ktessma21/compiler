#include <algorithm>
#include <assert.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <set>
#include <stdint.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include <utils.h>
#include <parser.h>
#include <spiller.h>
#include <liveness.h>
#include <interference.h>
#include <coloring.h>
// #include <generator.h>
#include <string.h>

void print_help(char *progName) {
  std::cerr << "Usage: " << progName
            << " [-v] [-g 0|1] [-O 0|1|2] [-s] [-l] [-i] SOURCE" << std::endl;
  return;
}

int main(int argc, char **argv) {
  auto enable_code_generator = true;
  auto spill_only = false;
  auto interference_only = false;
  auto liveness_only = false;
  int32_t optLevel = 3;

  /*
   * Check the compiler arguments.
   */
  Utils::verbose = false;
  if (argc < 2) {
    print_help(argv[0]);
    return 1;
  }
  int32_t opt;
  int64_t functionNumber = -1;
  while ((opt = getopt(argc, argv, "vg:O:slif:")) != -1) {
    switch (opt) {

    case 'l':
      liveness_only = true;
      break;

    case 'i':
      interference_only = true;
      break;

    case 's':
      spill_only = true;
      break;

    case 'O':
      optLevel = strtoul(optarg, NULL, 0);
      break;

    case 'f':
      functionNumber = strtoul(optarg, NULL, 0);
      break;

    case 'g':
      enable_code_generator = (strtoul(optarg, NULL, 0) == 0) ? false : true;
      break;

    case 'v':
      Utils::verbose = true;
      break;

    default:
      print_help(argv[0]);
      return 1;
    }
  }

  /*
   * Parse the input file.
   */

  char *fileName = argv[optind]; 


  if (spill_only) {
        /*
        * Parse an L2 function and the spill arguments, then spill.
        */
        auto spill = L2::parse_spill_file(fileName);
        auto str = L2::Spill(spill.function, spill.target, spill.prefix);
        std::cout << str << '\n';

        return 0;
    }

    if (liveness_only) {
        /*
        * Parse an L2 function and run liveness.
        */
       
        auto function = L2::parse_function_file(fileName);
        L2::LivenessPrint(function);

        return 0;
    }

    if (interference_only) {
        /*
        * Parse an L2 function and build the interference graph.
        */
        // std::cerr << "start parsing\n";
        auto function = L2::parse_function_file(fileName);
        auto graph = L2::Interference(function);
        graph.printItems();
  

        return 0;
    }

    if (functionNumber != -1) {
        /*
        * Print a single L2 function case.
        */
        auto program = L2::parse_file(fileName);
        std::cout <<  program.functions[functionNumber].to_string();

        return 0;
    }

  /*
  * Default: parse and compile the full L2 program.
  */
  auto program = L2::parse_file(fileName);
 
  // update each function of the program properly
  for (auto& f : program.functions){
      auto graph = L2::Interference(f);
      L2::GraphColoring(graph, f);
  }

  /*
   * Generate the target code normally.
   */
  if (enable_code_generator) {
    // TODO
    std::cerr << program.to_string();  
    

  }

  return 0;
}
