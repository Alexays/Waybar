#include "modules/image.hpp"

#include <json/value.h>

#include <config.hpp>

waybar::modules::Image::Image(const std::string& id, const Json::Value& config,
                              std::mutex& reap_mtx, std::list<pid_t>& reap)
    : AModule(config, "image", id, reap_mtx, reap) {
  strategy_ = getStrategy(id, config, MODULE_CLASS, event_box_, tooltipEnabled());

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
}

auto waybar::modules::Image::getStrategy(const std::string& id, const Json::Value& cfg,
                                         const std::string& module, Gtk::EventBox& evbox,
                                         bool hasTooltip)
    -> std::unique_ptr<waybar::modules::image::IStrategy> {
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
    // Do the blocking work (e.g. running a user script) here on the worker
    // thread; update() then only parses the result and draws on the main thread.
    strategy_->fetch();
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

void waybar::modules::Image::refresh(int sig) {
#ifdef SIGRTMIN
  if (config_["signal"].isInt() && sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
#endif
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

void MultipleImageStrategy::fetch() {
  // Run the (blocking) user script off the GTK main thread so the bar doesn't
  // freeze for the script's duration on every interval. update() consumes the
  // captured output. The static "entries" path takes priority and needs no exec.
  if (config_["entries"].empty() && !config_["exec"].empty()) {
    exec_output_ = util::command::exec(config_["exec"].asString(), "").out;
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
    try {
      pixbuf = Gdk::Pixbuf::create_from_file(path, size_, size_);
    } catch (const Glib::Error& e) {
      spdlog::error("failed to load image '{}': {}", path, std::string(e.what()));
      pixbuf.reset();  // fall through to the .empty branch
    }

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
  // Fire-and-forget: don't block the main loop waiting on the command's output.
  util::command::forkExec(data);
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
  if (config_["path"].isString()) {
    auto p = config_["path"].asString();
    auto result = Config::tryExpandPath(p, "");
    // Only use the expanded path when it resolves to exactly one existing match;
    // otherwise keep the literal path so paths with spaces/metacharacters still work.
    path_ = (result.size() == 1) ? result.front() : p;
  } else if (config_["exec"].isString()) {
    output_ = util::command::exec(config_["exec"].asString(), "");
    parseOutputRaw();
    // expand path if "~" or "$HOME" is present in original path
    auto result = Config::tryExpandPath(path_, "");
    path_ = (result.size() == 1) ? result.front() : path_;
  }

  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  if (Glib::file_test(path_, Glib::FILE_TEST_EXISTS)) {
    int scaled_icon_size = size_ * image_.get_scale_factor();
    try {
      pixbuf = Gdk::Pixbuf::create_from_file(path_, scaled_icon_size, scaled_icon_size);
    } catch (const Glib::Exception& e) {
      // Existing but corrupt/non-image file: degrade to the empty state instead of crashing.
      spdlog::warn("Failed to load image {}: {}", path_, std::string(e.what()));
      pixbuf.reset();
    }
  }

  if (pixbuf) {
    // Building a HiDPI-aware cairo surface requires a realized widget: it reads the
    // GdkWindow and its scale factor. During startup update() can run before the
    // Gtk::Image is realized, in which case get_window() is null; feeding that path a
    // null window aborts startup. Fall back to setting the pixbuf directly while the
    // widget is unrealized, and use the crisp surface path once a window is available.
    auto window = image_.get_window();
    if (window) {
      auto surface =
          Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image_.get_scale_factor(), window);
      image_.set(surface);
    } else {
      image_.set(pixbuf);
    }
    image_.show();

    if (hasTooltip_ && !tooltip_.empty()) {
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
