#include "modules/image.hpp"

#include <glibmm/exception.h>
#include <json/value.h>

#include <config.hpp>

namespace waybar::modules {

Image::Image(const std::string& id, const Json::Value& config)
    : AModule(config, "image", id, true), box_{Gtk::Box(Gtk::Orientation::HORIZONTAL, 0)} {
  w_ = &box_;
  box_.set_name(name_);
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);

  size_ = config["size"].asInt();
  if (size_ == 0) {
    size_ = 16;
  }

  strategy_ = getStrategy(box_, config, tooltipEnabled());

  const auto once = std::chrono::milliseconds::max();
  if (!config_.isMember("interval") || config_["interval"].isNull() ||
      config_["interval"] == "once") {
    interval_ = once;
  } else if (config_["interval"].isNumeric()) {
    const auto interval_seconds = config_["interval"].asDouble();
    if (interval_seconds <= 0) {
      interval_ = once;
    } else {
      interval_ =
          std::chrono::milliseconds(std::max(1L,  // Minimum 1ms due to millisecond precision
                                             static_cast<long>(interval_seconds * 1000)));
    }
  } else {
    interval_ = once;
  }

  delayWorker();
  bindEvents(box_);
}

auto Image::getStrategy(Gtk::Box& box, const Json::Value& config, bool hasTooltip)
    -> std::unique_ptr<image::IStrategy> {
  std::unique_ptr<image::IStrategy> strat;
  if (!config["multiple"].empty() && config["multiple"].asBool()) {
    strat = std::make_unique<image::MultipleImageStrategy>();
  } else {
    strat = std::make_unique<image::SingleImageStrategy>(box, hasTooltip);
  }

  return strat;
}

void Image::delayWorker() {
  thread_ = [this] {
    // Do the blocking work (e.g. running a user script) here on the worker
    // thread; update() then only parses the result and draws on the main thread.
    strategy_->doFetch(config_);
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

void Image::doRefresh(int sig) {
#ifdef SIGRTMIN
  if (config_["signal"].isInt() && sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
#endif
}

auto Image::doUpdate() -> void {
  strategy_->doUpdate(box_, config_, size_);

  AModule::doUpdate();
}

namespace image {

void MultipleImageStrategy::doFetch(const Json::Value& config) {
  // Run the (blocking) user script off the GTK main thread so the bar doesn't
  // freeze for the script's duration on every interval. update() consumes the
  // captured output. The static "entries" path takes priority and needs no exec.
  if (config["entries"].empty() && !config["exec"].empty()) {
    exec_output_ = util::command::exec(config["exec"].asString(), "").out;
  }
}

void MultipleImageStrategy::doUpdate(Gtk::Box& box, const Json::Value& config, int size) {
  // spdlog::info("update function run");

  // clear box_, previous css classes and memory
  if (box.get_children().size() > 0) {
    resetBoxAndMemory(box);
  }

  // set new images from config script
  if (!config["entries"].empty()) {
    setImagesData(config["entries"]);
  } else if (!config["exec"].empty()) {
    // exec output was captured by fetch() on the worker thread
    Json::Value as_json;
    Json::Reader reader;

    if (!reader.parse(exec_output_, as_json)) {
      spdlog::error("invalid json from exec {}", exec_output_);
      return;
    }

    setImagesData(as_json);
  } else {
    spdlog::error("no image files provded in config");
    return;
  }

  setupAndDraw(box, size);
}

void MultipleImageStrategy::setupAndDraw(Gtk::Box& box, int size) {
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
    try {
      pixbuf = Gdk::Pixbuf::create_from_file(path, size, size);
    } catch (const Glib::Error& e) {
      spdlog::error("failed to load image '{}': {}", path, std::string(e.what()));
      pixbuf.reset();  // fall through to the .empty branch
    }

    if (has_onclick) {
      auto btn = images_data_[i].btn;
      btn->set_name("button_" + path);
      btn->get_style_context()->add_class(marker);
      btn->set_tooltip_text(tooltip);
      btn->set_child(*img);
      box.append(*btn);

      btn->signal_clicked().connect(
          sigc::bind(sigc::mem_fun(*this, &MultipleImageStrategy::handleClick), data.on_click));

      if (pixbuf) {
        btn->show();
        img->set(pixbuf);
        box.get_style_context()->remove_class("empty");
      } else {
        btn->hide();
        img->clear();
        img->hide();
        box.get_style_context()->add_class("empty");
      }
    } else {
      img->set_name(path);
      img->get_style_context()->add_class(marker);
      img->set_tooltip_text(tooltip);
      box.append(*img);
      // spdlog::info("added image -> {}:{}", marker, path);

      if (pixbuf) {
        img->set(pixbuf);
        img->show();
        box.get_style_context()->remove_class("empty");
      } else {
        img->clear();
        img->hide();
        box.get_style_context()->add_class("empty");
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
        !Glib::file_test(path.asString(), Glib::FileTest::EXISTS)) {
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

void MultipleImageStrategy::resetBoxAndMemory(Gtk::Box& box) {
  auto children = box.get_children();
  for (auto child : children) {
    box.remove(*child);
    // spdlog::info("child removed with name -> {}", std::string(child->get_name()));
  }

  images_data_.clear();
}

void MultipleImageStrategy::handleClick(const Glib::ustring& data) {
  // Fire-and-forget: don't block the main loop waiting on the command's output.
  util::command::forkExec(data);
}

SingleImageStrategy::SingleImageStrategy(Gtk::Box& box, bool tooltipEnabled) : IStrategy() {
  hasTooltip_ = tooltipEnabled;

  box.append(image_);
}

void SingleImageStrategy::doUpdate(Gtk::Box& box, const Json::Value& config, int size) {
  if (config["path"].isString()) {
    auto p = config["path"].asString();
    auto result = Config::tryExpandPath(p, "");
    path_ = (result.size() == 1) ? result.front() : p;
  } else if (config["exec"].isString()) {
    output_ = util::command::exec(config["exec"].asString(), "");
    parseOutputRaw();
    auto result = Config::tryExpandPath(path_, "");
    path_ = (result.size() == 1) ? result.front() : path_;
  }

  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  if (Glib::file_test(path_, Glib::FileTest::EXISTS)) {
    int scaled_icon_size = size * image_.get_scale_factor();
    try {
      pixbuf = Gdk::Pixbuf::create_from_file(path_, scaled_icon_size, scaled_icon_size);
    } catch (const Glib::Exception& e) {
      spdlog::warn("Failed to load image {}: {}", path_, std::string(e.what()));
      pixbuf.reset();
    }
  }

  if (pixbuf) {
    // GTK4: Create a Gdk::Texture from the pixbuf and hand it to Gtk::Image.
    auto texture = Gdk::Texture::create_for_pixbuf(pixbuf);
    image_.set(texture);  // Gdk::Texture → Gdk::Paintable
    image_.show();

    if (hasTooltip_ && !tooltip_.empty()) {
      if (box.get_tooltip_markup() != tooltip_) {
        box.set_tooltip_markup(tooltip_);
      }
    }

    box.get_style_context()->remove_class("empty");
  } else {
    image_.clear();
    image_.hide();
    box.get_style_context()->add_class("empty");
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

}  // namespace image
}  // namespace waybar::modules
