#pragma once

#include <iostream>
#include <stdint.h>
#include <string>
// #include <cstdio>

namespace Utils {
extern bool verbose;

std::string fromNumberToString(int64_t n);

class IO {
public:
  static bool doesFileExist(char *fileName);
};

void printMessage(std::string s);
} // namespace Utils


#define TODO(msg) printf("Not implemented yet %s\n", msg);
