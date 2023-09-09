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

  EnumType parseStringToEnum(const std::string& str,
                             const std::map<std::string, EnumType>& enumMap) {
    // Convert the input string to uppercase
    std::string uppercaseStr = str;
    std::transform(uppercaseStr.begin(), uppercaseStr.end(), uppercaseStr.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    // Return enum match of string
    auto it = enumMap.find(uppercaseStr);
    if (it != enumMap.end()) return it->second;

    // Throw error if it doesnt return
    throw std::invalid_argument("Invalid string representation for enum");
  }

  ~EnumParser() = default;
};
}  // namespace waybar::util
