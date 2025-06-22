#pragma once

#include <iostream>
#include <string>

const std::string WHITESPACE = " \n\r\t\f\v";

inline std::string ltrim(const std::string& s) {
  size_t begin = s.find_first_not_of(WHITESPACE);
  return (begin == std::string::npos) ? "" : s.substr(begin);
}

inline std::string rtrim(const std::string& s) {
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

inline std::string trim(const std::string& s) { return rtrim(ltrim(s)); }

inline std::string capitalize(const std::string& str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return result;
}
