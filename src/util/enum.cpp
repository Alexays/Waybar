#include "util/enum.hpp"

#include <algorithm>  // for std::transform
#include <cctype>     // for std::toupper
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "modules/hyprland/workspaces.hpp"
#include "util/string.hpp"

namespace waybar::util {

template <typename EnumType>
EnumParser<EnumType>::EnumParser() = default;

template <typename EnumType>
EnumParser<EnumType>::~EnumParser() = default;

template <typename EnumType>
EnumType EnumParser<EnumType>::parseStringToEnum(const std::string& str,
                                                 const std::map<std::string, EnumType>& enumMap) {
  // Convert the input string to uppercase
  std::string uppercaseStr = capitalize(str);

  // Capitalize the map keys before searching
  std::map<std::string, EnumType> capitalizedEnumMap;
  std::transform(
      enumMap.begin(), enumMap.end(), std::inserter(capitalizedEnumMap, capitalizedEnumMap.end()),
      [](const auto& pair) { return std::make_pair(capitalize(pair.first), pair.second); });

  // Return enum match of string
  auto it = capitalizedEnumMap.find(uppercaseStr);
  if (it != capitalizedEnumMap.end()) return it->second;

  // Throw error if it doesn't return
  throw std::invalid_argument("Invalid string representation for enum");
}

// Explicit instantiations for specific EnumType types you intend to use
// Add explicit instantiations for all relevant EnumType types
template struct EnumParser<modules::hyprland::Workspaces::SortMethod>;

}  // namespace waybar::util
