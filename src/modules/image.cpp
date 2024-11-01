#include "modules/image.hpp"

namespace waybar::modules {

Image::Image(const std::string& id, const Json::Value& config)
    : AModule(config, "image", id), box_(Gtk::Orientation::HORIZONTAL, 0) {
  box_.append(image_);
  box_.set_name("image");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  AModule::bindEvents(box_);

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

void Image::delayWorker() {
  thread_ = [this] {
    dp.emit();
    auto interval{std::chrono::seconds(interval_)};
    thread_.sleep_for(interval);
  };
}

void Image::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

auto waybar::modules::Image::update() -> void {
  if (config_["path"].isString()) {
    path_ = config_["path"].asString();
  } else if (config_["exec"].isString()) {
    output_ = util::command::exec(config_["exec"].asString(), "");
    parseOutputRaw();
  } else {
    path_ = "";
  }

  if (Glib::file_test(path_, Glib::FileTest::EXISTS)) {
    image_.set(path_);
    image_.show();

    if (tooltipEnabled() && !tooltip_.empty()) {
      if (box_.get_tooltip_markup() != tooltip_) {
        box_.set_tooltip_markup(tooltip_);
      }
    }

    box_.get_style_context()->remove_class("empty");
  } else {
    image_.clear();
    image_.hide();
    box_.get_style_context()->add_class("empty");
  }

  AModule::update();
}

void Image::parseOutputRaw() {
  std::istringstream output(output_.out);
  std::string line;
  int i{0};
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

Image::operator Gtk::Widget&() { return box_; };

}  // namespace waybar::modules
