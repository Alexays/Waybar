#pragma once

#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
#else
#include <filesystem>
#endif
#include <fmt/format.h>
#include <fstream>
#include <regex>
#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules::sway {

#ifdef FILESYSTEM_EXPERIMENTAL
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

class Layout : public ALabel, public sigc::trackable {
 public:
  Layout(const std::string&, const Json::Value&);
  ~Layout() = default;
  auto update() -> void;

 private:
  static inline const fs::path xbk_file_ = "/usr/share/X11/xkb/rules/evdev.xml";

  void               onCmd(const struct Ipc::ipc_response&);
  void               onEvent(const struct Ipc::ipc_response&);
  void               worker();
  void               shortName();
  inline std::string sanitize(const std::string& text) {
    std::regex specialChars {R"([-[\]{}()*+?.,\^$|#\s])"};
    return std::regex_replace(text, specialChars, R"(\$&)");
  }

  std::string      layout_;
  std::string      short_description_;
  std::string      short_variant_;
  util::JsonParser parser_;
  std::mutex       mutex_;

  util::SleeperThread thread_;
  Ipc                 ipc_;
 };

}  // namespace waybar::modules::sway
