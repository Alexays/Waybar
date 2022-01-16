#include "modules/image.hpp"

#include <spdlog/spdlog.h>

waybar::modules::Image::Image(const std::string& name, const std::string& id,
                              const Json::Value& config)
    : AModule(config, "image-" + name, id, "{}") {
  event_box_.add(image_);

  dp.emit();

  path_ = config["path"].asString();
  size_ = config["size"].asInt();

  interval_ = config_["interval"].asInt();

  if (size_ == 0) {
    size_ = 16;
  }

  if (interval_ == 0) {
    interval_ = INT_MAX;
  }

  delayWorker();
}

void waybar::modules::Image::delayWorker() {
  thread_ = [this] {
    dp.emit();
    auto interval = std::chrono::seconds(interval_);
    thread_.sleep_for(interval);
  };
}

void waybar::modules::Image::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

auto waybar::modules::Image::update() -> void {
  Glib::RefPtr<Gdk::Pixbuf> pixbuf;

  if (Glib::file_test(path_, Glib::FILE_TEST_EXISTS))
    pixbuf = Gdk::Pixbuf::create_from_file(path_, size_, size_);
  else
    pixbuf = {};

  if (pixbuf) {
    image_.set(pixbuf);
    image_.show();
  } else {
    image_.clear();
    image_.hide();
  }

  AModule::update();
}
