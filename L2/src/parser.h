#pragma once

#include <l2.h>

namespace L2 {


	struct SpillInput {
        Program program;
        std::string target;   // e.g. "%var1"
        std::string prefix;   // e.g. "%S"
    };


	Program parse_file(char* fileName);

	SpillInput parse_spill_file(const char* fileName);
}
