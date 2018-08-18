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
    bool res =
      _reader->parse(data.c_str(), data.c_str() + data.size(), &root, &err);
    if (!res)
      throw std::runtime_error(err);
    return root;
  }

  ~JsonParser()
  {
    delete _reader;
  }

private:
  Json::CharReaderBuilder _builder;
  Json::CharReader *_reader;
};

}
