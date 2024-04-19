#pragma once

#include <fmt/format.h>
#include <gtkmm/image.h>
#include <spdlog/spdlog.h>

#include <csignal>
#include <string>
#include <utility>

#include "ALabel.hpp"
#include "AModule.hpp"
#include "glibmm/fileutils.h"
#include "gtkmm/box.h"
#include "gtkmm/button.h"
#include "gtkmm/image.h"
#include "gtkmm/widget.h"
#include "sigc++/adaptors/bind.h"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

struct ImageData {
  std::string path;
  std::string marker;
  std::string tooltip;
  std::string on_click;
  std::shared_ptr<Gtk::Image> img;
  std::shared_ptr<Gtk::Button> btn;
};

class Images : public AModule {
 public:
  Images(const std::string &, const Json::Value &);
  virtual ~Images() = default;
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void setImagesData(const Json::Value &);
  void setupAndDraw();
  void resetBoxAndMemory();
  void handleClick(const Glib::ustring &data);

  Json::Value config_;
  Gtk::Box box_;
  std::vector<ImageData> images_data_;
  int size_;
  int interval_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
