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
  output_ = {exit_code, ""};
  dp.emit();
}

void waybar::modules::Custom::workerOutputCallback(std::string output) {
  output_ = {0, std::move(output)};
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
  // Hide label if output is empty
  if ((config_["exec"].isString() || config_["exec-if"].isString()) &&
      (output_.out.empty() || output_.exit_code != 0)) {
    event_box_.hide();
  } else {
    if (config_["return-type"].asString() == "json") {
      parseOutputJson(output_.out);
    } else {
      parseOutputRaw(output_.out);
    }
    auto str = fmt::format(format_,
                           text_,
                           fmt::arg("alt", alt_),
                           fmt::arg("icon", getIcon(percentage_, alt_)),
                           fmt::arg("percentage", percentage_));
    if (str.empty()) {
      event_box_.hide();
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
    tooltip_ = parsed["tooltip"].asString();
    if (parsed["class"].isString()) {
      class_.push_back(parsed["class"].asString());
    } else if (parsed["class"].isArray()) {
      for (auto const& c : parsed["class"]) {
        class_.push_back(c.asString());
      }
    }
    if (!parsed["percentage"].asString().empty() && parsed["percentage"].isUInt()) {
      percentage_ = parsed["percentage"].asUInt();
    } else {
      percentage_ = 0;
    }
    break;
  }
}
