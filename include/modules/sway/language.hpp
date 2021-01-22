#pragma once

#include <fmt/format.h>
#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Language : public ALabel, public sigc::trackable {
 public:
  Language(const std::string& id, const Json::Value& config);
  ~Language() = default;
  auto update() -> void;

 private:
  void onEvent(const struct Ipc::ipc_response&);
  void onCmd(const struct Ipc::ipc_response&);
  std::string getIcon(const std::string& key);
  
  std::string      lang_;
  util::JsonParser parser_;
  std::mutex       mutex_;
  Ipc              ipc_;
};

}  // namespace waybar::modules::sway
