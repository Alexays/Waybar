#pragma once

#include <json/json.h>

namespace waybar::util {

struct JsonParser {
  JsonParser() : reader_(builder_.newCharReader()) {}

  const Json::Value parse(const std::string& data, std::size_t size = 0) const {
    Json::Value root(Json::objectValue);
    if (data.empty()) {
      return root;
    }
    std::string err;
    auto data_size = size > 0 ? size : data.size();
    bool res = reader_->parse(data.c_str(), data.c_str() + data_size, &root, &err);
    if (!res) throw std::runtime_error(err);
    return root;
  }

  ~JsonParser() = default;

 private:
  Json::CharReaderBuilder                 builder_;
  std::unique_ptr<Json::CharReader> const reader_;
};

}  // namespace waybar::util
