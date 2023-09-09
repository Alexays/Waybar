#pragma once

#include <cctype>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "util/string.hpp"

namespace waybar::util {

template <typename EnumType>
struct EnumParser {
  EnumParser() {}

  EnumType parseStringToEnum(const std::string& str,
                             const std::map<std::string, EnumType>& enumMap) {
    // Convert the input string to uppercase
    std::string uppercaseStr = capitalize(str);

    // Capitalize the map keys before searching
    std::map<std::string, EnumType> capitalizedEnumMap;
    std::transform(
        enumMap.begin(), enumMap.end(), std::inserter(capitalizedEnumMap, capitalizedEnumMap.end()),
        [this](const auto& pair) { return std::make_pair(capitalize(pair.first), pair.second); });

    // Return enum match of string
    auto it = enumMap.find(uppercaseStr);
    if (it != enumMap.end()) return it->second;

    // Throw error if it doesnt return
    throw std::invalid_argument("Invalid string representation for enum");
  }

  ~EnumParser() = default;
};
}  // namespace waybar::util
