#pragma once

#include <l2.h>

namespace L2 {


	struct SpillInput {
        Function function;
        std::string target;   
        std::string prefix;   
    };


	L2::Program parse_file(char* fileName);

	SpillInput parse_spill_file(const char* fileName);

    L2::Function parse_l2_function(const char* fileName);
}
