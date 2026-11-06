#pragma once

#include <fmt/format.h>
#include <gtkmm/image.h>

#include <csignal>
#include <mutex>
#include <string>
#include <vector>

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
  // spawning a user script, decoding an image) so the GTK main loop isn't stalled.
  virtual void fetch() {}
  // Returns false when fetch() has to run again
  virtual bool update() = 0;
};

class SingleImageStrategy : public IStrategy {
 public:
  SingleImageStrategy(const std::string&, const Json::Value&, const std::string&, Gtk::EventBox&,
                      bool);
  ~SingleImageStrategy() override = default;
  void fetch() override;
  bool update() override;

 private:
  void parseOutputRaw();

  util::command::res output_;
  Json::Value config_;
  Gtk::Image image_;
  std::string path_;
  int size_;
  Gtk::Box box_;
  bool hasTooltip_;
  // Shared by fetch() on the worker thread and update()
  std::mutex mutex_;
  std::string tooltip_;
  Glib::RefPtr<Gdk::Pixbuf> pixbuf_;
  int pixbuf_scale_factor_;
  int scale_factor_;
};

class MultipleImageStrategy : public IStrategy {
 public:
  MultipleImageStrategy(const std::string&, const Json::Value&, const std::string&, Gtk::EventBox&);
  ~MultipleImageStrategy() override = default;
  void fetch() override;
  bool update() override;

 private:
  struct ImageData {
    std::string path;
    std::string marker;
    std::string tooltip;
    std::string on_click;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    std::shared_ptr<Gtk::Image> img;
    std::shared_ptr<Gtk::Button> btn;
  };

  static void setImagesData(const Json::Value&, std::vector<ImageData>&);
  void setupAndDraw();
  void resetBoxAndMemory();
  void handleClick(const Glib::ustring& data);

  Json::Value config_;
  int size_;
  Gtk::Box box_;
  std::vector<ImageData> images_data_;
  // Decoded by fetch() on the worker thread and drawn by update()
  std::mutex mutex_;
  std::vector<ImageData> fetched_images_data_;
};

}  // namespace image

class Image : public AModule {
 public:
  Image(const std::string&, const Json::Value&);
  virtual ~Image() = default;
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void handleEvent();
  static std::unique_ptr<image::IStrategy> getStrategy(const std::string&, const Json::Value&,
                                                       const std::string&, Gtk::EventBox&, bool);

  std::chrono::milliseconds interval_;
  std::unique_ptr<image::IStrategy> strategy_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
