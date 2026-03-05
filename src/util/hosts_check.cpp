#include "util/hosts_check.hpp"

#include <glibmm/miscutils.h>
#include <json/config.h>

namespace waybar::util {
bool valid_host(const Json::Value& config) {
  if (config.isMember("hosts") && config["hosts"].isArray()) {
    const auto hostname = Glib::get_host_name();

    if (!std::ranges::any_of(config["hosts"].begin(), config["hosts"].end(),
                             [&](const auto& h) { return h.asString() == hostname; }))
      return false;
  }
  return true;
}
}  // namespace waybar::util
