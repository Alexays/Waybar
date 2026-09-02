#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>

#include "util/string.hpp"

namespace waybar::util {

template <typename EnumType>
//struct EnumParser {
  EnumType parseStringToEnum(const std::string& str,
                             const std::map<std::string, EnumType>& enumMap) {
    std::string uppercaseStr = capitalize(str);
    std::map<std::string, EnumType> capitalizedEnumMap;
    std::transform(
        enumMap.begin(), enumMap.end(),
        std::inserter(capitalizedEnumMap, capitalizedEnumMap.end()),
        [](const auto& pair) {
          return std::make_pair(capitalize(pair.first), pair.second);
        });

    auto it = capitalizedEnumMap.find(uppercaseStr);
    if (it != capitalizedEnumMap.end()) return it->second;

    throw std::invalid_argument("Invalid string representation for enum");
//  }
}

}  // namespace waybar::util
