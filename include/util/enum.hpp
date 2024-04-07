#pragma once

#include <map>
#include <stdexcept>
#include <string>

namespace wabar::util {

template <typename EnumType>
struct EnumParser {
 public:
  EnumParser();
  ~EnumParser();

  EnumType parseStringToEnum(const std::string& str,
                             const std::map<std::string, EnumType>& enumMap);
};

}  // namespace wabar::util
