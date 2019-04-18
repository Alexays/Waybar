#pragma once

#include <json/json.h>

namespace waybar::util {

struct JsonParser {
  JsonParser() : reader_(builder_.newCharReader()) {}

  const Json::Value parse(const std::string& data) const {
    Json::Value root;
    std::string err;
    if (data.empty()) {
      return root;
    }
    bool res = reader_->parse(data.c_str(), data.c_str() + data.size(), &root, &err);
    if (!res) throw std::runtime_error(err);
    return root;
  }

  ~JsonParser() = default;

 private:
  Json::CharReaderBuilder                 builder_;
  std::unique_ptr<Json::CharReader> const reader_;
};

}  // namespace waybar::util
