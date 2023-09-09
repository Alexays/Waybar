#pragma once

#include <cctype>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace waybar::util {

template <typename EnumType>
struct EnumParser {
  EnumParser() {}

  EnumType sortStringToEnum(const std::string& str,
                            const std::map<std::string, EnumType>& enumMap) {
    // Convert the input string to uppercase
    std::string uppercaseStr;
    for (char c : str) {
      uppercaseStr += std::toupper(c);
    }

    auto it = enumMap.find(uppercaseStr);
    if (it != enumMap.end()) {
      return it->second;
    } else {
      throw std::invalid_argument("Invalid string representation for enum");
    }
  }

  ~EnumParser() = default;
};
}  // namespace waybar::util
