#include "modules/config.hpp"

namespace waybar::modules {

Config::Config(const Json::Value &config, const std::string &format)
    : config_{config},
      format_{config_["format"].isString() ? config_["format"].asString() : format},
      format_default_{format_},
      format_alt_{config_["format-alt"].isString() ? config_["format-alt"].asString() : ""},
      format_icons_{config_["format-icons"]} {};

std::string Config::getIcon(uint16_t percentage, const std::string &alt, uint16_t max) {
  auto result{format_icons_};

  if (result.isObject()) {
    if (!alt.empty() && (result[alt].isString() || result[alt].isArray()))
      result = result[alt];
    else
      result = result["default"];
  }
  if (result.isArray()) {
    auto size{result.size()};
    if (size) {
      auto idx{std::clamp(percentage / ((max == 0 ? 100 : max) / size), 0U, size - 1)};
      result = result[idx];
    }
  }
  if (result.isString())
    return result.asString();
  else
    return "";
}

std::string Config::getIcon(uint16_t percentage, const std::vector<std::string> &alts,
                            uint16_t max) {
  auto result{format_icons_};

  if (result.isObject()) {
    std::string _alt{"default"};
    for (const auto &alt : alts)
      if (!alt.empty() && (result[alt].isString() || result[alt].isArray())) {
        _alt = alt.c_str();
        break;
      }
    result = result[_alt];
  }
  if (result.isArray()) {
    auto size{result.size()};
    if (size) {
      auto idx{std::clamp(percentage / ((max == 0 ? 100 : max) / size), 0U, size - 1)};
      result = result[idx];
    }
  }
  if (result.isString())
    return result.asString();
  else
    return "";
}

auto waybar::modules::Config::switchFormatAlt() -> void {
  alt_ = !alt_;
  format_ = (alt_) ? format_alt_ : format_default_;
}

auto waybar::modules::Config::doAction(const Glib::ustring &name) -> void {
  const std::map<const std::string, void (waybar::modules::Config::*const)()>::const_iterator &rec{
      actionMap_.find(name)};
  if (rec != actionMap_.cend()) {
    (this->*rec->second)();
  }
}

bool waybar::modules::Config::doActionExists(const Glib::ustring &name) {
  return actionMap_.find(name) != actionMap_.cend();
}

}  // namespace waybar::modules
