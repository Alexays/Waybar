#pragma once

#include <gtkmm/image.h>

#include "AModule.hpp"
#include "gtkmm/box.h"
#include "util/command.hpp"
#include "util/sleeper_thread.hpp"


namespace waybar::modules {

class Image final : public AModule {
 public:
  Image(const std::string&, const Json::Value&);
  virtual ~Image() = default;
  auto update() -> void override;
  void refresh(int /*signal*/) override;
  operator Gtk::Widget &() override;

 private:
  void delayWorker();
  void handleEvent();
  void parseOutputRaw();

  Gtk::Box box_;
  Gtk::Image image_;
  std::string path_;
  Glib::ustring tooltip_;
  int size_;
  int interval_;
  util::command::res output_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
