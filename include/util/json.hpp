#pragma once

#include <json/json.h>

namespace waybar::util {

struct JsonParser {

  JsonParser()
    : _reader(_builder.newCharReader())
  {}

  Json::Value parse(const std::string data)
  {
    Json::Value root;
    std::string err;
    if (_reader == nullptr) {
      throw std::runtime_error("Unable to parse");
    }
    bool res =
      _reader->parse(data.c_str(), data.c_str() + data.size(), &root, &err);
    if (!res)
      throw std::runtime_error(err);
    return root;
  }

  ~JsonParser()
  {
    delete _reader;
    _reader = nullptr;
  }

private:
  Json::CharReaderBuilder _builder;
  Json::CharReader *_reader;
};

}
