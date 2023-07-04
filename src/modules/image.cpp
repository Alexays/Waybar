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
  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  if (config_["path"].isString()) {
    path_ = config_["path"].asString();
  } else if (config_["exec"].isString()) {
    output_ = util::command::exec(config_["exec"].asString());
    parseOutputRaw();
  } else {
    path_ = "";
  }
  if (Glib::file_test(path_, Glib::FILE_TEST_EXISTS))
    pixbuf = Gdk::Pixbuf::create_from_file(path_, size_, size_);
  else
    pixbuf = {};

  if (pixbuf) {
    if (tooltipEnabled() && !tooltip_.empty()) {
      if (box_.get_tooltip_markup() != tooltip_) {
        box_.set_tooltip_markup(tooltip_);
      }
    }
    image_.set(pixbuf);
    image_.show();
  } else {
    image_.clear();
    image_.hide();
  }

  AModule::update();
}

void waybar::modules::Image::parseOutputRaw() {
  std::istringstream output(output_.out);
  std::string line;
  int i = 0;
  while (getline(output, line)) {
    if (i == 0) {
      path_ = line;
    } else if (i == 1) {
      tooltip_ = line;
    } else {
      break;
    }
    i++;
  }
}
