#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

inline std::string toLower(const std::string& str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

// Finds the last occurrence of `c` that is not nested inside parentheses,
// scanning from the end. Unbalanced brackets are tolerated: a stray '(' never
// pushes the depth below zero, and a stray ')' only shields what precedes it.
// Returns std::string::npos when there is no such occurrence.
inline size_t rfindOutsideParens(std::string_view s, char c) {
  int depth = 0;
  for (size_t i = s.size(); i-- > 0;) {
    if (s[i] == ')') {
      ++depth;
    } else if (s[i] == '(') {
      if (depth > 0) --depth;
    } else if (s[i] == c && depth == 0) {
      return i;
    }
  }
  return std::string::npos;
}

inline std::vector<std::string> split(std::string_view s, std::string_view delimiter,
                                      int max_splits = -1) {
  std::vector<std::string> result;
  size_t pos = 0;
  size_t next_pos = 0;
  while ((next_pos = s.find(delimiter, pos)) != std::string::npos) {
    result.push_back(std::string(s.substr(pos, next_pos - pos)));
    pos = next_pos + delimiter.size();
    if (max_splits > 0 && result.size() == static_cast<size_t>(max_splits)) {
      break;
    }
  }
  result.push_back(std::string(s.substr(pos)));
  return result;
}
