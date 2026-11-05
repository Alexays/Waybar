#include "modules/image.hpp"

#include <fcntl.h>
#include <gdkmm/pixbufloader.h>
#include <json/value.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <config.hpp>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

#include "util/tooltip.hpp"

waybar::modules::Image::Image(const std::string& id, const Json::Value& config)
    : AModule(config, "image", id) {
  strategy_ = getStrategy(id, config, MODULE_CLASS, event_box_, tooltipEnabled());
  enableChildMultiClick();

  const auto once = std::chrono::milliseconds::max();
  if (!config_.isMember("interval") || config_["interval"].isNull() ||
      config_["interval"] == "once") {
    interval_ = once;
  } else if (config_["interval"].isNumeric()) {
    const auto interval_seconds = config_["interval"].asDouble();
    if (interval_seconds <= 0) {
      interval_ = once;
    } else {
      interval_ = std::chrono::milliseconds(
          static_cast<long>(std::clamp(interval_seconds, 0.001, 1e9) * 1000));
    }
  } else {
    interval_ = once;
  }

  delayWorker();
}

auto waybar::modules::Image::getStrategy(const std::string& id, const Json::Value& cfg,
                                         const std::string& module, util::ModuleBox& evbox,
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
    // Run the user script and decode the images here on the worker thread;
    // update() then only draws the result on the main thread.
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
  if (!strategy_->update()) {
    thread_.wake_up();
  }

  AModule::update();
}

namespace waybar::modules::image {

// Decodes the file at `path` scaled to fit in size x size, on the worker thread; empty on failure.
static Glib::RefPtr<Gdk::Pixbuf> loadImage(const std::string& path, int size) {
  constexpr off_t max_file_size = 64 << 20;
  constexpr int64_t max_pixels = 1 << 26;
  // SleeperThread::stop() cancels the worker: never inside gdk-pixbuf or with the fd open
  util::CancellationGuard cancel_lock;

  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    return {};
  }
  // A FIFO or a device could block forever; O_NONBLOCK and fstat() cover a swap after stat()
  if (!S_ISREG(st.st_mode)) {
    spdlog::warn("Failed to load image {}: not a regular file", path);
    return {};
  }
  int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    spdlog::warn("Failed to load image {}: {}", path, strerror(errno));
    return {};
  }
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > max_file_size) {
    spdlog::warn("Failed to load image {}: not a regular file of at most 64 MiB", path);
    ::close(fd);
    return {};
  }

  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  bool too_large = false;
  try {
    auto loader = Gdk::PixbufLoader::create();
    // Scale as gdk_pixbuf_new_from_file_at_size() does; refuse a huge picture before its decode
    loader->signal_size_prepared().connect([&](int width, int height) {
      if (width <= 0 || height <= 0 || static_cast<int64_t>(width) * height > max_pixels) {
        spdlog::warn("Failed to load image {}: it declares {}x{} pixels", path, width, height);
        too_large = true;
        loader->set_size(0, 0);
      } else if (height > width) {
        loader->set_size(std::max(1, static_cast<int>(0.5 + 1.0 * width * size / height)), size);
      } else {
        loader->set_size(size, std::max(1, static_cast<int>(0.5 + 1.0 * height * size / width)));
      }
    });
    guint8 buf[65536];
    ssize_t n = 0;
    while (!too_large && (n = ::read(fd, buf, sizeof(buf))) > 0) {
      loader->write(buf, static_cast<gsize>(n));
    }
    loader->close();
    if (!too_large && n == 0) {
      pixbuf = loader->get_pixbuf();
    }
  } catch (const Glib::Error& e) {
    if (!too_large) {
      spdlog::warn("Failed to load image {}: {}", path, std::string(e.what()));
    }
  }
  ::close(fd);
  return pixbuf;
}

// 0 or no value selects the default; the upper bound keeps size * scale factor inside an int.
static int configSize(const Json::Value& size) {
  if (size.isNull() || (size.isInt() && size.asInt() == 0)) {
    return 16;
  }
  if (size.isInt() && size.asInt() > 0 && size.asInt() <= 8192) {
    return size.asInt();
  }
  spdlog::warn("image: 'size' must be an integer between 1 and 8192, using the default 16");
  return 16;
}

MultipleImageStrategy::MultipleImageStrategy(const std::string& id, const Json::Value& config,
                                             const std::string& module, util::ModuleBox& evbox)
    : IStrategy(), box_(Gtk::Orientation::HORIZONTAL, 0) {
  config_ = config;

  box_.set_name("image");
  box_.add_css_class(id);
  box_.add_css_class(module);
  // The images expand inside the box, not the module
  box_.set_hexpand(false);
  evbox.set_child(box_);

  size_ = configSize(config["size"]);
}

void MultipleImageStrategy::fetch() {
  // The static "entries" path takes priority and needs no exec.
  std::vector<ImageData> images_data;
  if (!config_["entries"].empty()) {
    setImagesData(config_["entries"], images_data);
  } else if (!config_["exec"].empty()) {
    auto exec_output = util::command::exec(config_["exec"].asString(), "").out;
    Json::Value as_json;
    Json::Reader reader;

    if (reader.parse(exec_output, as_json)) {
      setImagesData(as_json, images_data);
    } else {
      spdlog::error("invalid json from exec {}", exec_output);
    }
  } else {
    spdlog::error("no image files provded in config");
  }

  for (auto& data : images_data) {
    data.pixbuf = loadImage(data.path, size_);
  }

  std::lock_guard<std::mutex> lock(mutex_);
  fetched_images_data_.swap(images_data);
}

bool MultipleImageStrategy::update() {
  resetBoxAndMemory();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    images_data_ = fetched_images_data_;
  }
  setupAndDraw();
  return true;
}

void MultipleImageStrategy::setupAndDraw() {
  for (unsigned int i = 0; i < images_data_.size(); i++) {
    images_data_[i].img = std::make_shared<util::ScaledImage>();
    images_data_[i].btn = std::make_shared<Gtk::Button>();

    auto img = images_data_[i].img;
    auto data = images_data_[i];

    auto path = data.path;
    auto marker = data.marker;
    auto tooltip = data.tooltip;
    util::limit_tooltip_text(tooltip);
    bool has_onclick = !data.on_click.empty();
    auto pixbuf = data.pixbuf;

    if (has_onclick) {
      auto btn = images_data_[i].btn;
      btn->set_name("button_" + path);
      btn->add_css_class(marker);
      btn->set_tooltip_text(tooltip);
      btn->set_child(*img);
      btn->add_css_class("image-button");
      btn->set_hexpand(true);
      box_.append(*btn);

      btn->signal_clicked().connect(
          sigc::bind(sigc::mem_fun(*this, &MultipleImageStrategy::handleClick), data.on_click));

      if (pixbuf) {
        img->set_pixbuf(pixbuf, 1);
        box_.remove_css_class("empty");
      } else {
        btn->set_visible(false);
        img->clear();
        img->set_visible(false);
        box_.add_css_class("empty");
      }
    } else {
      img->set_name(path);
      img->add_css_class(marker);
      img->set_tooltip_text(tooltip);
      img->set_hexpand(true);
      box_.append(*img);

      if (pixbuf) {
        img->set_pixbuf(pixbuf, 1);
        box_.remove_css_class("empty");
      } else {
        img->clear();
        img->set_visible(false);
        box_.add_css_class("empty");
      }
    }
  }
}

void MultipleImageStrategy::setImagesData(const Json::Value& entries,
                                          std::vector<ImageData>& images_data) {
  if (!entries.isArray()) {
    spdlog::error("invalid input in images config -> {}", entries);
    return;
  }
  for (unsigned int i = 0; i < entries.size(); i++) {
    // Indexing anything but an object by key throws
    if (!entries[i].isObject()) {
      spdlog::error("invalid input in images config -> {}", entries[i]);
      return;
    }
    auto path = entries[i]["path"];
    auto marker = entries[i]["marker"];
    auto tooltip = entries[i]["tooltip"];
    auto onclick = entries[i]["on-click"];

    bool has_tooltip_err = !tooltip.isNull() && !tooltip.isString();
    bool has_onclick_err = !onclick.isNull() && !onclick.isString();

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

    images_data.push_back(std::move(data));
  }
}

void MultipleImageStrategy::resetBoxAndMemory() {
  while (auto* child = box_.get_first_child()) {
    box_.remove(*child);
  }

  images_data_.clear();
}

void MultipleImageStrategy::handleClick(const Glib::ustring& data) {
  // Fire-and-forget: don't block the main loop waiting on the command's output.
  util::command::forkExec(data);
}

SingleImageStrategy::SingleImageStrategy(const std::string& id, const Json::Value& config,
                                         const std::string& module, util::ModuleBox& evbox,
                                         bool tooltipEnabled)
    : IStrategy(), box_(Gtk::Orientation::HORIZONTAL, 0) {
  config_ = config;
  hasTooltip_ = tooltipEnabled;

  box_.append(image_);
  // The image expands inside the box, not the module
  image_.set_hexpand(true);
  box_.set_hexpand(false);
  box_.set_name("image");
  if (!id.empty()) {
    box_.add_css_class(id);
  }
  box_.add_css_class(module);
  evbox.set_child(box_);

  size_ = configSize(config["size"]);
  scale_factor_ = pixbuf_scale_factor_ = image_.get_scale_factor();
}

// Script output is data: unlike Config::tryExpandPath(), never hand it to wordexp().
static std::string expandHome(const std::string& path) {
  for (const std::string_view prefix : {"~", "$HOME", "${HOME}"}) {
    if (path.starts_with(prefix) && (path.size() == prefix.size() || path[prefix.size()] == '/')) {
      return Glib::get_home_dir() + path.substr(prefix.size());
    }
  }
  return path;
}

void SingleImageStrategy::fetch() {
  if (config_["path"].isString()) {
    auto p = config_["path"].asString();
    auto result = Config::tryExpandPath(p, "");
    // Only use the expanded path when it resolves to exactly one existing match;
    // otherwise keep the literal path so paths with spaces/metacharacters still work.
    path_ = (result.size() == 1) ? result.front() : p;
  } else if (config_["exec"].isString()) {
    output_ = util::command::exec(config_["exec"].asString(), "");
    std::lock_guard<std::mutex> lock(mutex_);
    parseOutputRaw();
    path_ = expandHome(path_);
  }

  std::unique_lock<std::mutex> lock(mutex_);
  const int scale_factor = scale_factor_;
  lock.unlock();
  auto pixbuf = loadImage(path_, size_ * scale_factor);
  lock.lock();
  pixbuf_.swap(pixbuf);
  pixbuf_scale_factor_ = scale_factor;
}

bool SingleImageStrategy::update() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Only the main thread may ask GTK; a changed scale factor makes fetch() decode again
  scale_factor_ = image_.get_scale_factor();

  if (pixbuf_) {
    image_.set_pixbuf(pixbuf_, pixbuf_scale_factor_);
    image_.set_visible(true);

    if (hasTooltip_ && !tooltip_.empty()) {
      // ustring::operator!= collates, which crashes on invalid UTF-8; compare bytes
      if (box_.get_tooltip_markup().raw() != tooltip_) {
        box_.set_tooltip_markup(tooltip_);
      }
    }

    box_.remove_css_class("empty");
  } else {
    image_.clear();
    image_.set_visible(false);
    box_.add_css_class("empty");
  }
  return scale_factor_ == pixbuf_scale_factor_;
}

void SingleImageStrategy::parseOutputRaw() {
  std::istringstream output(output_.out);
  std::string line;
  int i = 0;
  while (getline(output, line)) {
    if (i == 0) {
      path_ = line;
    } else if (i == 1) {
      // Script output can be invalid UTF-8, which g_utf8_* functions do not accept
      Glib::ustring validated_line = line;
      if (!validated_line.validate()) {
        validated_line = validated_line.make_valid();
      }
      tooltip_ = validated_line.raw();
      util::limit_tooltip_markup(tooltip_);
    } else {
      break;
    }
    i++;
  }
}

}  // namespace waybar::modules::image
