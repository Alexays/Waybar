#pragma once

#include <json/json.h>

#include <AModule.hpp>

namespace waybar {

class Bar;

class Factory {
 public:
  Factory(const Bar& bar, const Json::Value& config);
  AModule* makeModule(const std::string& name, const std::string& pos) const;

 private:
  const Bar& bar_;
  const Json::Value& config_;
};

}  // namespace waybar
