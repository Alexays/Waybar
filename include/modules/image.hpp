#pragma once

#include <fmt/format.h>
#include <gtkmm/image.h>

#include <csignal>
#include <string>

#include "ALabel.hpp"
#include "gtkmm/box.h"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Image : public AModule {
 public:
  Image(const std::string&, const Json::Value&, std::mutex& reap_mtx, std::list<pid_t>& reap);
  virtual ~Image() = default;
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void handleEvent();
  void parseOutputRaw();

  Gtk::Box box_;
  Gtk::Image image_;
  std::string path_;
  std::string tooltip_;
  int size_;
  std::chrono::milliseconds interval_;
  util::command::res output_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
