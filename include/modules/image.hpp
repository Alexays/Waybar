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

namespace image {

class IStrategy {
 public:
  virtual ~IStrategy() = default;
  virtual void update() = 0;
};

class SingleImageStrategy : public IStrategy {
 public:
  SingleImageStrategy(const std::string &, const Json::Value &, const std::string &,
                      Gtk::EventBox &, bool);
  ~SingleImageStrategy() override = default;
  void update() override;

 private:
  void parseOutputRaw();

  util::command::res output_;
  Json::Value config_;
  Gtk::Image image_;
  std::string path_;
  std::string tooltip_;
  int size_;
  Gtk::Box box_;
  bool hasTooltip_;
};

class MultipleImageStrategy : public IStrategy {
 public:
  MultipleImageStrategy(const std::string &, const Json::Value &, const std::string &,
                        Gtk::EventBox &);
  ~MultipleImageStrategy() override = default;
  void update() override;

 private:
  struct ImageData {
    std::string path;
    std::string marker;
    std::string tooltip;
    std::string on_click;
    std::shared_ptr<Gtk::Image> img;
    std::shared_ptr<Gtk::Button> btn;
  };

  void setImagesData(const Json::Value &);
  void setupAndDraw();
  void resetBoxAndMemory();
  void handleClick(const Glib::ustring &data);

  Json::Value config_;
  int size_;
  Gtk::Box box_;
  std::vector<ImageData> images_data_;
};

}  // namespace image

class Image : public AModule {
 public:
  Image(const std::string &, const Json::Value &);
  virtual ~Image() = default;
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void handleEvent();
  static std::unique_ptr<image::IStrategy> getStrategy(const std::string &, const Json::Value &,
                                                       const std::string &, Gtk::EventBox &, bool);

  int interval_;
  std::unique_ptr<image::IStrategy> strategy_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
