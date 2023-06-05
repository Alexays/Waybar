#pragma once

#include "DBusClient.hpp"

namespace waybar::modules {

class Config {
 public:
  Config(const Json::Value &config, const std::string &format);
  virtual ~Config() = default;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);
  virtual std::string getIcon(uint16_t, const std::vector<std::string> &alts, uint16_t max = 0);
  auto doAction(const Glib::ustring &name = "") -> void;
  bool doActionExists(const Glib::ustring &name);
 protected:
  const Json::Value &config_;
  std::string format_;
  std::string format_default_;
  bool alt_{false};
 private:
  const std::string format_alt_;
  const Json::Value format_icons_;

  auto switchFormatAlt() -> void;
  const static inline std::map<const std::string, void (waybar::modules::Config::*const)()> actionMap_ {
    {"format-alt-click", &waybar::modules::Config::switchFormatAlt}
  };
};
} // namespace waybar
