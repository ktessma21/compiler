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
  if (spill_only) {

    /*
     * Parse an L2 function and the spill arguments.
     */
    // TODO

  } else if (liveness_only) {

    /*
     * Parse an L2 function.
     */
    // TODO

  } else if (interference_only) {

    /*
     * Parse an L2 function.
     */
    // TODO

  } else {

    /*
     * Parse the L2 program.
     */
    // TODO
  }

  /*
   * Special cases.
   */
  if (spill_only) {

    /*
     * Spill.
     */
    // TODO

    return 0;
  }

  /*
   * Liveness test.
   */
  if (liveness_only) {
    // TODO

    return 0;
  }

  /*
   * Interference graph test.
   */
  if (interference_only) {
    // TODO

    return 0;
  }

  /*
   * Print a single L2 function case.
   */
  if (functionNumber != -1) {
    // TODO

    return 0;
  }

  /*
   * Generate the target code normally.
   */
  if (enable_code_generator) {
    // TODO
  }

  return 0;
}
