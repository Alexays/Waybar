#include "modules/custom.hpp"

#include <spdlog/spdlog.h>

#include <utility>

#include "util/scope_guard.hpp"

waybar::modules::Custom::Custom(const std::string& name, const std::string& id,
                                const Json::Value& config, const std::string& output_name)
    : AIconLabel(config, "custom-" + name, id, "{}"),
      name_(name),
      output_name_(output_name),
      id_(id),
      tooltip_format_enabled_{config_["tooltip-format"].isString()},
      percentage_(0),
      fp_(nullptr),
      pid_(-1) {
  if (config.isNull()) {
    spdlog::warn("There is no configuration for 'custom/{}', element will be hidden", name);
  }
  dp.emit();
  if (!config_["signal"].empty() && config_["interval"].empty() &&
      config_["restart-interval"].empty()) {
    waitingWorker();
  } else if (interval_.count() > 0) {
    delayWorker();
  } else if (config_["exec"].isString()) {
    continuousWorker();
  }
  if (config_["image-path"].isString()) {
    image_path_ = config_["image-path"].asString();
  }
  if (config_["image-name"].isString()) {
    image_name_ = config_["image-name"].asString();
  }
  if (config["icon-size"].isUInt()) {
    app_icon_size_ = config["icon-size"].asUInt();
  }
}

waybar::modules::Custom::~Custom() {
  if (pid_ != -1) {
    killpg(pid_, SIGTERM);
    waitpid(pid_, NULL, 0);
    pid_ = -1;
  }
}

void waybar::modules::Custom::delayWorker() {
  if (!config_["exec"].isString() && !config_["exec-if"].isString()) {
    dp.emit();
    return;
  }

  thread_ = [this] {
    for (int i : this->pid_children_) {
      int status;
      waitpid(i, &status, 0);
    }

    this->pid_children_.clear();

    bool can_update = true;
    if (config_["exec-if"].isString()) {
      output_ = util::command::execNoRead(config_["exec-if"].asString());
      if (output_.exit_code != 0) {
        can_update = false;
        dp.emit();
      }
    }
    if (can_update) {
      if (config_["exec"].isString()) {
        output_ = util::command::exec(config_["exec"].asString(), output_name_);
      }
      dp.emit();
    }
    thread_.sleep_for(interval_);
  };
}

void waybar::modules::Custom::continuousWorker() {
  auto cmd = config_["exec"].asString();
  pid_ = -1;
  fp_ = util::command::open(cmd, pid_, output_name_);
  if (!fp_) {
    throw std::runtime_error("Unable to open " + cmd);
  }
  thread_ = [this, cmd] {
    char* buff = nullptr;
    waybar::util::ScopeGuard buff_deleter([&buff]() {
      if (buff) {
        free(buff);
      }
    });
    size_t len = 0;
    if (getline(&buff, &len, fp_) == -1) {
      int exit_code = 1;
      if (fp_) {
        exit_code = WEXITSTATUS(util::command::close(fp_, pid_));
        fp_ = nullptr;
      }
      if (exit_code != 0) {
        output_ = {exit_code, ""};
        dp.emit();
        spdlog::error("{} stopped unexpectedly, is it endless?", name_);
      }
      if (config_["restart-interval"].isNumeric() && config_["restart-interval"].asDouble() > 0) {
        pid_ = -1;
        thread_.sleep_for(std::chrono::milliseconds(
            std::max(1L,  // Minimum 1ms due to millisecond precision
                     static_cast<long>(config_["restart-interval"].asDouble() * 1000))));
        fp_ = util::command::open(cmd, pid_, output_name_);
        if (!fp_) {
          throw std::runtime_error("Unable to open " + cmd);
        }
      } else {
        // A non-positive restart-interval must not busy-respawn the script
        // (that starves the GTK main loop); treat it as "do not restart".
        thread_.stop();
        return;
      }
    } else {
      std::string output = buff;

      // Remove last newline
      if (!output.empty() && output[output.length() - 1] == '\n') {
        output.erase(output.length() - 1);
      }
      output_ = {0, output};
      dp.emit();
    }
  };
}

void waybar::modules::Custom::waitingWorker() {
  thread_ = [this] {
    bool can_update = true;
    if (config_["exec-if"].isString()) {
      output_ = util::command::execNoRead(config_["exec-if"].asString());
      if (output_.exit_code != 0) {
        can_update = false;
        dp.emit();
      }
    }
    if (can_update) {
      if (config_["exec"].isString()) {
        output_ = util::command::exec(config_["exec"].asString(), output_name_);
      }
      dp.emit();
    }
    thread_.sleep();
  };
}

void waybar::modules::Custom::refresh(int sig) {
#ifdef SIGRTMIN
  if (config_["signal"].isInt() && sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
#endif
}

void waybar::modules::Custom::handleEvent() {
  if (!config_["exec-on-event"].isBool() || config_["exec-on-event"].asBool()) {
    thread_.wake_up();
  }
}

bool waybar::modules::Custom::handleScroll(GdkEventScroll* e) {
  auto ret = ALabel::handleScroll(e);
  handleEvent();
  return ret;
}

bool waybar::modules::Custom::handleToggle(GdkEventButton* const& e) {
  auto ret = ALabel::handleToggle(e);
  handleEvent();
  return ret;
}

auto waybar::modules::Custom::update() -> void {
  // Hide label if output is empty
  if ((config_["exec"].isString() || config_["exec-if"].isString()) &&
      (output_.out.empty() || output_.exit_code != 0)) {
    event_box_.hide();
  } else {
    if (config_["return-type"].asString() == "json") {
      parseOutputJson();
    } else {
      parseOutputRaw();
    }

    try {
      auto str = fmt::format(fmt::runtime(format_), fmt::arg("text", text_), fmt::arg("alt", alt_),
                             fmt::arg("icon", getIcon(percentage_, alt_)),
                             fmt::arg("percentage", percentage_));
      if ((config_["hide-empty-text"].asBool() && text_.empty()) ||
          (str.empty() && image_path_.empty() && image_name_.empty())) {
        event_box_.hide();
      } else {
        setLabelMarkup(str);
        if (tooltipEnabled()) {
          std::string tooltip_markup;
          if (tooltip_format_enabled_) {
            auto tooltip = config_["tooltip-format"].asString();
            tooltip_markup = fmt::format(fmt::runtime(tooltip), fmt::arg("text", text_),
                                         fmt::arg("tooltip", tooltip_), fmt::arg("alt", alt_),
                                         fmt::arg("icon", getIcon(percentage_, alt_)),
                                         fmt::arg("percentage", percentage_));
          } else if (text_ == tooltip_) {
            tooltip_markup = str;
          } else {
            tooltip_markup = tooltip_;
          }

          setTooltipMarkup(tooltip_markup);
        }
        auto style = label_.get_style_context();
        auto classes = style->list_classes();
        for (auto const& c : classes) {
          if (c == id_) continue;
          style->remove_class(c);
        }
        for (auto const& c : class_) {
          style->add_class(c);
        }
        // Mirror the dynamic script classes onto box_, which now carries the
        // #custom-<name> widget name (see AIconLabel), so #custom-<name>.<class>
        // CSS selectors keep resolving as they did in 0.15.0.
        auto box_style = box_.get_style_context();
        for (auto const& c : box_style->list_classes()) {
          if (c == id_ || c == MODULE_CLASS) continue;
          box_style->remove_class(c);
        }
        for (auto const& c : class_) {
          box_style->add_class(c);
        }
        style->add_class("flat");
        style->add_class("text-button");
        style->add_class(MODULE_CLASS);
        auto image_style = image_.get_style_context();
        image_style->add_class("image-button");
        event_box_.show();
        if (!image_path_.empty()) {
          try {
            auto pixbuf =
                Gdk::Pixbuf::create_from_file(image_path_, app_icon_size_, app_icon_size_);
            image_.set(pixbuf);
          } catch (const Glib::Error& e) {
            spdlog::warn("custom {}: failed to load image-path '{}': {}", name_, image_path_,
                         std::string(e.what()));
            image_.clear();
          }
        } else if (!image_name_.empty()) {
          image_.set_from_icon_name(image_name_, Gtk::ICON_SIZE_INVALID);
          image_.set_pixel_size(app_icon_size_);
        }

        label_.set_visible(!str.empty());
      }
    } catch (const fmt::format_error& e) {
      if (std::strcmp(e.what(), "cannot switch from manual to automatic argument indexing") != 0)
        throw;

      throw fmt::format_error(
          "mixing manual and automatic argument indexing is no longer supported; "
          "try replacing \"{}\" with \"{text}\" in your format specifier");
    }
  }
  // Call parent update
  AIconLabel::update();

  // Show a configured image-path/image-name image after the base update() so
  // AIconLabel::update()'s icon gate cannot re-hide it. Leave the embedded-icon
  // and "icon" cases to the base class (they have no image-path/image-name).
  if (!image_name_.empty() || !image_path_.empty()) {
    image_.set_visible(true);
  }
}

void waybar::modules::Custom::parseOutputRaw() {
  std::istringstream output(output_.out);
  std::string line;
  int i = 0;
  while (getline(output, line)) {
    Glib::ustring validated_line = line;
    if (!validated_line.validate()) {
      validated_line = validated_line.make_valid();
    }

    if (i == 0) {
      if (config_["escape"].isBool() && config_["escape"].asBool()) {
        text_ = Glib::Markup::escape_text(validated_line);
        tooltip_ = Glib::Markup::escape_text(validated_line);
      } else {
        text_ = validated_line;
        tooltip_ = validated_line;
      }
      tooltip_ = validated_line;
      class_.clear();
    } else if (i == 1) {
      if (config_["escape"].isBool() && config_["escape"].asBool()) {
        tooltip_ = Glib::Markup::escape_text(validated_line);
      } else {
        tooltip_ = validated_line;
      }
    } else if (i == 2) {
      class_.push_back(validated_line);
    } else {
      break;
    }
    i++;
  }
}

void waybar::modules::Custom::parseOutputJson() {
  std::istringstream output(output_.out);
  std::string line;
  class_.clear();
  // A script can emit invalid UTF-8; passing it unchecked to Pango/GTK aborts
  // the whole bar in g_utf8_* (see parseOutputRaw, which validates the same way).
  auto sanitize = [](const std::string& s) -> Glib::ustring {
    Glib::ustring value = s;
    if (!value.validate()) {
      value = value.make_valid();
    }
    return value;
  };
  while (getline(output, line)) {
    auto parsed = parser_.parse(line);
    const bool escape = config_["escape"].isBool() && config_["escape"].asBool();
    if (escape) {
      text_ = Glib::Markup::escape_text(sanitize(parsed["text"].asString()));
    } else {
      text_ = sanitize(parsed["text"].asString());
    }
    if (escape) {
      alt_ = Glib::Markup::escape_text(sanitize(parsed["alt"].asString()));
    } else {
      alt_ = sanitize(parsed["alt"].asString());
    }
    if (escape) {
      tooltip_ = Glib::Markup::escape_text(sanitize(parsed["tooltip"].asString()));
    } else {
      tooltip_ = sanitize(parsed["tooltip"].asString());
    }
    if (parsed["class"].isString()) {
      class_.push_back(parsed["class"].asString());
    } else if (parsed["class"].isArray()) {
      for (auto const& c : parsed["class"]) {
        class_.push_back(c.asString());
      }
    }

    if (!parsed["percentage"].asString().empty() && parsed["percentage"].isNumeric()) {
      percentage_ = (int)lround(parsed["percentage"].asFloat());
    } else {
      percentage_ = 0;
    }
    break;
  }
}
