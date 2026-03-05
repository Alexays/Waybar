#include <glibmm/miscutils.h>

#include "util/hosts_check.hpp"

namespace waybar::util {
// replaces ``<>&"'`` with their encoded counterparts
bool valid_host(const Json::Value& config) {
  if (config.isMember("hosts") && config["hosts"].isArray()) {
    auto hostname = Glib::get_host_name();
    for (const auto& host : config["hosts"]) {
      if (host.asString() == hostname) {
        return true;
      }
    }
    return false;
  }
  return true;
}
}  // namespace waybar::util
