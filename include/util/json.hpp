#pragma once

#include <fmt/ostream.h>
#include <json/json.h>

#include <algorithm>
#include <codecvt>
#include <iostream>
#include <locale>
#include <regex>

#if (FMT_VERSION >= 90000)

template <>
struct fmt::formatter<Json::Value> : ostream_formatter {};

#endif

namespace waybar::util {

class JsonParser {
 public:
  JsonParser() = default;

  Json::Value parse(const std::string& jsonStr) {
    Json::Value root;

    // replace all occurrences of "\x" with "\u00", because JSON doesn't allow "\x" escape sequences
    std::string modifiedJsonStr = replaceHexadecimalEscape(jsonStr);

    std::istringstream jsonStream(modifiedJsonStr);
    std::string errs;
    if (!Json::parseFromStream(m_readerBuilder, jsonStream, &root, &errs)) {
      throw std::runtime_error("Error parsing JSON: " + errs);
    }
    return root;
  }

 private:
  Json::CharReaderBuilder m_readerBuilder;

  static std::string replaceHexadecimalEscape(const std::string& str) {
    static std::regex re("\\\\x");
    return std::regex_replace(str, re, "\\u00");
  }
};
}  // namespace waybar::util
