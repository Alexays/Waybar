#pragma once

#include <iostream>
#include <map>
#include <string>

namespace waybar::util {

struct EnumParser {
  EnumParser() {}

  enum SORT_METHOD { ID, NAME, NUMBER, DEFAULT };

  SORT_METHOD sortStringToEnum(const std::string& str) {
    static const std::map<std::string, SORT_METHOD> enumMap = {
        {"ID", ID}, {"NAME", NAME}, {"NUMBER", NUMBER}, {"DEFAULT", DEFAULT}};

    auto it = enumMap.find(str);
    if (it != enumMap.end()) {
      return it->second;
    } else {
      throw std::invalid_argument("Invalid string representation for enum");
    }
  }

  ~EnumParser() = default;
};
}  // namespace waybar::util
