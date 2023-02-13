#include "modules/image.hpp"

waybar::modules::Image::Image(const std::string& id, const Json::Value& config)
    : AModule(config, "image", id), box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.pack_start(image_);
  box_.set_name("image");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);

  dp.emit();

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
  util::command::res output_;

  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  if (config_["path"].isString()) {
    path_ = config_["path"].asString();
  } else if (config_["exec"].isString()) {
    output_ = util::command::exec(config_["exec"].asString());
    path_ = output_.out;
  } else {
    path_ = "";
  }
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
