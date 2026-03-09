#include "modules/custom.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

waybar::modules::Custom::Custom(const std::string& name, const std::string& id,
                                const Json::Value& config, const std::string& output_name)
    : ALabel(config, "custom-" + name, id, "{}"),
      name_(name),
      output_name_(output_name),
      id_(id),
      tooltip_format_enabled_{config_["tooltip-format"].isString()},
      percentage_(0) {
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
}

waybar::modules::Custom::~Custom() {
  restart_connection_.disconnect();
  if (continuous_stream_) {
    continuous_stream_->stop();
  }
}

void waybar::modules::Custom::delayWorker() {
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
  continuous_stream_ = std::make_unique<util::command::LineStream>(
      output_name_,
      [this](const std::string& output) {
        output_ = {0, output};
        dp.emit();
      },
      [this](int exit_code) { handleContinuousProcessExit(exit_code); });
  startContinuousProcess(true);
}

void waybar::modules::Custom::startContinuousProcess(bool throw_on_failure) {
  const auto cmd = config_["exec"].asString();

  try {
    continuous_stream_->start(cmd);
  } catch (const Glib::SpawnError& e) {
    if (throw_on_failure) {
      throw std::runtime_error("Unable to open " + cmd + ": " + e.what().raw());
    }
    output_ = {1, ""};
    dp.emit();
    spdlog::error("Unable to restart {}: {}", name_, e.what().raw());
    scheduleContinuousRestart();
  } catch (const std::exception& e) {
    if (throw_on_failure) {
      throw;
    }
    output_ = {1, ""};
    dp.emit();
    spdlog::error("Unable to restart {}: {}", name_, e.what());
    scheduleContinuousRestart();
  }
}

void waybar::modules::Custom::handleContinuousProcessExit(int exit_code) {
  if (exit_code != 0) {
    output_ = {exit_code, ""};
    dp.emit();
    spdlog::error("{} stopped unexpectedly, is it endless?", name_);
  }

  if (config_["restart-interval"].isNumeric()) {
    scheduleContinuousRestart();
  }
}

void waybar::modules::Custom::scheduleContinuousRestart() {
  restart_connection_.disconnect();
  restart_connection_ = Glib::signal_timeout().connect(
      [this] {
        startContinuousProcess(false);
        return false;
      },
      std::max(1U, static_cast<unsigned>(config_["restart-interval"].asDouble() * 1000)));
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
  if (config_["signal"].isInt() && sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
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
      if ((config_["hide-empty-text"].asBool() && text_.empty()) || str.empty()) {
        event_box_.hide();
      } else {
        label_.set_markup(str);
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

          if (last_tooltip_markup_ != tooltip_markup) {
            label_.set_tooltip_markup(tooltip_markup);
            last_tooltip_markup_ = std::move(tooltip_markup);
          }
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
        style->add_class("flat");
        style->add_class("text-button");
        style->add_class(MODULE_CLASS);
        event_box_.show();
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
  ALabel::update();
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
  while (getline(output, line)) {
    auto parsed = parser_.parse(line);
    if (config_["escape"].isBool() && config_["escape"].asBool()) {
      text_ = Glib::Markup::escape_text(parsed["text"].asString());
    } else {
      text_ = parsed["text"].asString();
    }
    if (config_["escape"].isBool() && config_["escape"].asBool()) {
      alt_ = Glib::Markup::escape_text(parsed["alt"].asString());
    } else {
      alt_ = parsed["alt"].asString();
    }
    if (config_["escape"].isBool() && config_["escape"].asBool()) {
      tooltip_ = Glib::Markup::escape_text(parsed["tooltip"].asString());
    } else {
      tooltip_ = parsed["tooltip"].asString();
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
