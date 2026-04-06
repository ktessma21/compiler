#include <fstream>
#include <iostream>
#include <sstream>

#include <utils.h>

bool Utils::verbose;

std::string Utils::fromNumberToString(int64_t n) {
  std::ostringstream oss;
  oss << n;
  return oss.str();
}

void Utils::printMessage(std::string s) {
  if (Utils::verbose) {
    std::cout << s;
  }
  return;
}

bool Utils::IO::doesFileExist(char *fileName) {
  std::ofstream tmpFile;
  bool e;
  tmpFile.open(fileName, std::ios::in);
  if (!tmpFile.is_open()) {
    e = false;
  } else {
    e = true;
  }
  tmpFile.close();

  return e;
}
