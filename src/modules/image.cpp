#include "modules/image.hpp"

#include <json/value.h>

waybar::modules::Image::Image(const std::string& id, const Json::Value& config)
    : AModule(config, "image", id) {
  strategy_ = getStrategy(id, config, MODULE_CLASS, event_box_, tooltipEnabled());

  interval_ = config_["interval"].asInt();

  if (interval_ == 0) {
    interval_ = INT_MAX;
  }

  delayWorker();
}

auto waybar::modules::Image::getStrategy(
    const std::string& id, const Json::Value& cfg, const std::string& module, Gtk::EventBox& evbox,
    bool hasTooltip) -> std::unique_ptr<waybar::modules::image::IStrategy> {
  std::unique_ptr<waybar::modules::image::IStrategy> strat;
  if (!cfg["multiple"].empty() && cfg["multiple"].asBool()) {
    strat = std::make_unique<waybar::modules::image::MultipleImageStrategy>(id, cfg, module, evbox);
  } else {
    strat = std::make_unique<waybar::modules::image::SingleImageStrategy>(id, cfg, module, evbox,
                                                                          hasTooltip);
  }

  return strat;
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
  strategy_->update();

  AModule::update();
}

namespace waybar::modules::image {

MultipleImageStrategy::MultipleImageStrategy(const std::string& id, const Json::Value& config,
                                             const std::string& module, Gtk::EventBox& evbox)
    : IStrategy(), box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  config_ = config;

  box_.set_name("image");
  box_.get_style_context()->add_class(id);
  box_.get_style_context()->add_class(module);
  evbox.add(box_);

  size_ = config["size"].asInt();
  if (size_ == 0) {
    size_ = 16;
  }
}

void MultipleImageStrategy::update() {
  // spdlog::info("update function run");

  // clear box_, previous css classes and memory
  if (box_.get_children().size() > 0) {
    resetBoxAndMemory();
  }

  // set new images from config script
  if (!config_["entries"].empty()) {
    setImagesData(config_["entries"]);
  } else if (!config_["exec"].empty()) {
    auto exec = util::command::exec(config_["exec"].asString(), "");
    Json::Value as_json;
    Json::Reader reader;

    if (!reader.parse(exec.out, as_json)) {
      spdlog::error("invalid json from exec {}", exec.out);
      return;
    }

    setImagesData(as_json);
  } else {
    spdlog::error("no image files provded in config");
    return;
  }

  setupAndDraw();
}

void MultipleImageStrategy::setupAndDraw() {
  for (unsigned int i = 0; i < images_data_.size(); i++) {
    images_data_[i].img = std::make_shared<Gtk::Image>();
    images_data_[i].btn = std::make_shared<Gtk::Button>();

    auto img = images_data_[i].img;
    auto data = images_data_[i];

    auto path = data.path;
    auto marker = data.marker;
    auto tooltip = data.tooltip;
    bool has_onclick = !data.on_click.empty();

    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    pixbuf = Gdk::Pixbuf::create_from_file(path, size_, size_);

    if (has_onclick) {
      auto btn = images_data_[i].btn;
      btn->set_name("button_" + path);
      btn->get_style_context()->add_class(marker);
      btn->set_tooltip_text(tooltip);
      btn->set_image(*img);
      box_.pack_start(*btn);

      btn->add_events(Gdk::BUTTON_PRESS_MASK);
      btn->signal_clicked().connect(
          sigc::bind(sigc::mem_fun(*this, &MultipleImageStrategy::handleClick), data.on_click));

      if (pixbuf) {
        btn->show_all();
        img->set(pixbuf);
        box_.get_style_context()->remove_class("empty");
      } else {
        btn->hide();
        img->clear();
        img->hide();
        box_.get_style_context()->add_class("empty");
      }
    } else {
      img->set_name(path);
      img->get_style_context()->add_class(marker);
      img->set_tooltip_text(tooltip);
      box_.pack_start(*img);
      // spdlog::info("added image -> {}:{}", marker, path);

      if (pixbuf) {
        img->set(pixbuf);
        img->show();
        box_.get_style_context()->remove_class("empty");
      } else {
        img->clear();
        img->hide();
        box_.get_style_context()->add_class("empty");
      }
    }
  }
}

void MultipleImageStrategy::setImagesData(const Json::Value& entries) {
  for (unsigned int i = 0; i < entries.size(); i++) {
    auto path = entries[i]["path"];
    auto marker = entries[i]["marker"];
    auto tooltip = entries[i]["tooltip"];
    auto onclick = entries[i]["on-click"];

    bool has_tooltip_err = !tooltip.empty() && !tooltip.isString();
    bool has_onclick_err = !onclick.empty() && !onclick.isString();

    if (!path.isString() || !marker.isString() || has_tooltip_err || has_onclick_err ||
        !Glib::file_test(path.asString(), Glib::FILE_TEST_EXISTS)) {
      spdlog::error("invalid input in images config -> {}", entries[i]);
      return;
    }
    ImageData data;
    data.path = path.asString();
    data.marker = marker.asString();
    data.tooltip = !tooltip.empty() ? tooltip.asString() : "";
    data.on_click = onclick.asString();

    images_data_.push_back(data);
  }
}

void MultipleImageStrategy::resetBoxAndMemory() {
  auto children = box_.get_children();
  for (auto child : children) {
    box_.remove(*child);
    // spdlog::info("child removed with name -> {}", std::string(child->get_name()));
  }

  images_data_.clear();
}

void MultipleImageStrategy::handleClick(const Glib::ustring& data) {
  auto msg = std::string(data);

  auto exec = util::command::exec(data, "");
}

SingleImageStrategy::SingleImageStrategy(const std::string& id, const Json::Value& config,
                                         const std::string& module, Gtk::EventBox& evbox,
                                         bool tooltipEnabled)
    : IStrategy(), box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  config_ = config;
  hasTooltip_ = tooltipEnabled;

  box_.pack_start(image_);
  box_.set_name("image");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(module);
  evbox.add(box_);

  size_ = config["size"].asInt();
  if (size_ == 0) {
    size_ = 16;
  }
}

void SingleImageStrategy::update() {
  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  if (config_["path"].isString()) {
    path_ = config_["path"].asString();
  } else if (config_["exec"].isString()) {
    output_ = util::command::exec(config_["exec"].asString(), "");
    parseOutputRaw();
  } else {
    path_ = "";
  }
  if (Glib::file_test(path_, Glib::FILE_TEST_EXISTS))
    pixbuf = Gdk::Pixbuf::create_from_file(path_, size_, size_);
  else
    pixbuf = {};

  if (pixbuf) {
    if (hasTooltip_ && !tooltip_.empty()) {
      if (box_.get_tooltip_markup() != tooltip_) {
        box_.set_tooltip_markup(tooltip_);
      }
    }
    image_.set(pixbuf);
    image_.show();
    box_.get_style_context()->remove_class("empty");
  } else {
    image_.clear();
    image_.hide();
    box_.get_style_context()->add_class("empty");
  }
}

void SingleImageStrategy::parseOutputRaw() {
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

}  // namespace waybar::modules::image
