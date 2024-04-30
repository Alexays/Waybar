#pragma once

#include <json/json.h>

#include <AModule.hpp>

namespace waybar {

class Bar;

class Factory {
 public:
  Factory(const Bar& bar, const Json::Value& config);
  AModule* addModule(const std::string& name, const std::string& pos);
  std::vector<std::shared_ptr<waybar::AModule>> modules_all_;

 private:
  const Bar& bar_;
  const Json::Value& config_;
  AModule* makeModule(const std::string& name, const std::string& pos, waybar::Factory& factory) const;
};

}  // namespace waybar
