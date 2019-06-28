#pragma once

#include "util/command.hpp"
#include "util/json.hpp"

namespace waybar::util::condshow {

/** Extract the `show-if` field from the given config and execute it as a command if possible.
 * Returns `true` if the return code is 0 or the `show-if` field isn't a string, returns `false`
 * otherwise.
 */
inline bool show_module(const Json::Value& config, const std::string args) {
  if (config["show-if"].isString()) {
    std::ostringstream command;
    command << config["show-if"].asString() << " " << args;
    auto res = util::command::exec(command.str());
    return res.exit_code == 0;
  }
  return true;
}

}  // namespace waybar::util::condshow
