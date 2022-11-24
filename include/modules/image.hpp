#pragma once

#include <fmt/format.h>
#include <gtkmm/image.h>

#include <csignal>
#include <string>

#include "ALabel.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Image : public AModule {
 public:
  Image(const std::string&, const std::string&, const Json::Value&);
  auto update() -> void;
  void refresh(int /*signal*/);

 private:
  void delayWorker();
  void handleEvent();

  Gtk::Image  image_;
  std::string path_;
  int         size_;
  int         interval_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
