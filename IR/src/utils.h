#pragma once

#include <iostream>
#include <stdint.h>
#include <string>

namespace Utils {
extern bool verbose;

std::string fromNumberToString(int64_t n);

class IO {
public:
  static bool doesFileExist(char *fileName);
};

void printMessage(std::string s);
} // namespace Utils
