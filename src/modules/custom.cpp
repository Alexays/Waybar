#include "modules/custom.hpp"

#include <spdlog/spdlog.h>

waybar::modules::Custom::Custom(const std::string& name, const std::string& id,
                                const Json::Value& config)
    : ALabel(config, "custom-" + name, id, "{}"),
      name_(name),
      thread_(
          config, [this](std::string output) { workerOutputCallback(std::move(output)); },
          [this](int exit_code) { workerExitCallback(exit_code); }) {
  dp.emit();
}

void waybar::modules::Custom::workerExitCallback(int exit_code) {
  {
    std::lock_guard<std::mutex> guard(output_mutex_);
    output_ = exit_code;
  }
  dp.emit();
}

void waybar::modules::Custom::workerOutputCallback(std::string output) {
  {
    std::lock_guard<std::mutex> guard(output_mutex_);
    output_ = std::move(output);
  }
  dp.emit();
}

void waybar::modules::Custom::injectOutput(Json::Value output) {
  {
    std::lock_guard<std::mutex> guard(output_mutex_);
    output_ = std::move(output);
  }
  dp.emit();
}

void waybar::modules::Custom::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
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
  std::variant<std::monostate, int, std::string, Json::Value> output;
  {
    std::lock_guard<std::mutex> guard(output_mutex_);
    output = std::move(output_);
    output_ = std::monostate();
  }

  // Hide label if output is empty
  bool hide = config_["exec"].isString() || config_["exec-if"].isString();

  if (std::holds_alternative<std::monostate>(output)) {
    if (hide) {
      // No changes since the last update, do nothing.
      ALabel::update();
      return;
    }
  } else if (std::holds_alternative<int>(output)) {
    // The exit code is non-zero if we get here, so do nothing to hide the label.
  } else if (std::holds_alternative<std::string>(output)) {
    const std::string& s = std::get<std::string>(output);
    if (!s.empty()) {
      hide = false;
      if (config_["return-type"].asString() == "json") {
        parseOutputJson(s);
      } else {
        parseOutputRaw(s);
      }
    }
  } else {
    hide = false;
    const Json::Value& value = std::get<Json::Value>(output);
    if (value.isString()) {
      parseOutputRaw(value.asString());
    } else {
      handleOutputJson(value);
    }
  }

  if (!hide) {
    auto str = fmt::format(format_,
                           text_,
                           fmt::arg("alt", alt_),
                           fmt::arg("icon", getIcon(percentage_, alt_)),
                           fmt::arg("percentage", percentage_));
    if (str.empty()) {
      hide = true;
    } else {
      label_.set_markup(str);
      if (tooltipEnabled()) {
        if (text_ == tooltip_) {
          if (label_.get_tooltip_markup() != str) {
            label_.set_tooltip_markup(str);
          }
        } else {
          if (label_.get_tooltip_markup() != tooltip_) {
            label_.set_tooltip_markup(tooltip_);
          }
        }
      }
      auto classes = label_.get_style_context()->list_classes();
      for (auto const& c : classes) {
        label_.get_style_context()->remove_class(c);
      }
      for (auto const& c : class_) {
        label_.get_style_context()->add_class(c);
      }
      event_box_.show();
    }
  }

  if (hide) {
    event_box_.hide();
  }

  // Call parent update
  ALabel::update();
}

void waybar::modules::Custom::parseOutputRaw(const std::string& output_str) {
  std::istringstream output(output_str);
  std::string        line;
  int                i = 0;
  while (getline(output, line)) {
    if (i == 0) {
      if (config_["escape"].isBool() && config_["escape"].asBool()) {
        text_ = Glib::Markup::escape_text(line);
      } else {
        text_ = line;
      }
      tooltip_ = line;
      class_.clear();
    } else if (i == 1) {
      tooltip_ = line;
    } else if (i == 2) {
      class_.push_back(line);
    } else {
      break;
    }
    i++;
  }
}

void waybar::modules::Custom::parseOutputJson(const std::string& output_str) {
  std::istringstream output(output_str);
  std::string        line;
  class_.clear();
  while (getline(output, line)) {
    auto parsed = parser_.parse(line);
    handleOutputJson(parsed);
    break;
  }
}

void waybar::modules::Custom::handleOutputJson(const Json::Value& value) {
  if (config_["escape"].isBool() && config_["escape"].asBool()) {
    text_ = Glib::Markup::escape_text(value["text"].asString());
  } else {
    text_ = value["text"].asString();
  }
  if (config_["escape"].isBool() && config_["escape"].asBool()) {
    alt_ = Glib::Markup::escape_text(value["alt"].asString());
  } else {
    alt_ = value["alt"].asString();
  }
  tooltip_ = value["tooltip"].asString();
  if (value["class"].isString()) {
    class_.push_back(value["class"].asString());
  } else if (value["class"].isArray()) {
    for (auto const& c : value["class"]) {
      class_.push_back(c.asString());
    }
  }
  if (!value["percentage"].asString().empty() && value["percentage"].isUInt()) {
    percentage_ = value["percentage"].asUInt();
  } else {
    percentage_ = 0;
  }
}
