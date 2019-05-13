#pragma once

#include <json/json.h>

namespace waybar::util {

struct JsonParser {
  JsonParser() {}

  const Json::Value parse(const std::string& data) const {
    Json::Value root(Json::objectValue);
    if (data.empty()) {
      return root;
    }
    std::unique_ptr<Json::CharReader> const reader(builder_.newCharReader());
    std::string                             err;
    bool res = reader->parse(data.c_str(), data.c_str() + data.size(), &root, &err);
    if (!res) throw std::runtime_error(err);
    return root;
  }

  ~JsonParser() = default;

 private:
  Json::CharReaderBuilder builder_;
};

}  // namespace waybar::util
