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
  // Runs on the worker thread before update(). Use it for blocking work (e.g.
  // spawning a user script) so the GTK main loop isn't stalled. Default no-op.
  virtual void doFetch(const Json::Value& config) {}
  virtual void doUpdate(Gtk::Box& box, const Json::Value& config, int size) = 0;
};

class SingleImageStrategy final : public IStrategy {
 public:
  SingleImageStrategy(Gtk::Box& box, bool tooltipEnabled);
  ~SingleImageStrategy() override = default;
  void doUpdate(Gtk::Box& box, const Json::Value& config, int size) override;

 private:
  void parseOutputRaw();

  util::command::res output_;
  Gtk::Image image_;
  std::string path_;
  std::string tooltip_;
  bool hasTooltip_;
};

class MultipleImageStrategy final : public IStrategy {
 public:
  ~MultipleImageStrategy() override = default;
  void doFetch(const Json::Value& config) override;
  void doUpdate(Gtk::Box& box, const Json::Value& config, int size) override;

 private:
  struct ImageData {
    std::string path;
    std::string marker;
    std::string tooltip;
    std::string on_click;
    std::shared_ptr<Gtk::Image> img;
    std::shared_ptr<Gtk::Button> btn;
  };

  void setImagesData(const Json::Value&);
  void setupAndDraw(Gtk::Box& box, int size);
  void resetBoxAndMemory(Gtk::Box& box);
  void handleClick(const Glib::ustring& data);

  std::vector<ImageData> images_data_;
  // stdout captured by fetch() on the worker thread and consumed by update()
  std::string exec_output_;
};

}  // namespace image

class Image final : public AModule {
 public:
  Image(const std::string&, const Json::Value&);
  virtual ~Image() = default;
  auto doUpdate() -> void override;
  void doRefresh(int /*signal*/) override;

 private:
  void delayWorker();
  void handleEvent();
  static std::unique_ptr<image::IStrategy> getStrategy(Gtk::Box& box, const Json::Value& config,
                                                       bool hasTooltip);

  std::chrono::milliseconds interval_;
  std::unique_ptr<image::IStrategy> strategy_;
  util::SleeperThread thread_;
  Gtk::Box box_;
  int size_;
};

}  // namespace waybar::modules
