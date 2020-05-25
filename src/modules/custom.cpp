#include "modules/custom.hpp"

#include <spdlog/spdlog.h>

waybar::modules::Custom::Custom(const std::string& name, const std::string& id,
                                const Json::Value& config)
    : ALabel(config, "custom-" + name, id, "{}"), name_(name), fp_(nullptr), pid_(-1) {
  dp.emit();
  if (interval_.count() > 0) {
    delayWorker();
  } else if (config_["exec"].isString()) {
    continuousWorker();
  }
}

waybar::modules::Custom::~Custom() {
  if (pid_ != -1) {
    killpg(pid_, SIGTERM);
    pid_ = -1;
  }
}

void waybar::modules::Custom::delayWorker() {
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
        output_ = util::command::exec(config_["exec"].asString());
      }
      dp.emit();
    }
    thread_.sleep_for(interval_);
  };
}

void waybar::modules::Custom::continuousWorker() {
  auto cmd = config_["exec"].asString();
  pid_ = -1;
  fp_ = util::command::open(cmd, pid_);
  if (!fp_) {
    throw std::runtime_error("Unable to open " + cmd);
  }
  thread_ = [this, cmd] {
    char*  buff = nullptr;
    size_t len = 0;
    bool   restart = false;
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
      if (config_["restart-interval"].isUInt()) {
        restart = true;
        pid_ = -1;
        fp_ = util::command::open(cmd, pid_);
        if (!fp_) {
          throw std::runtime_error("Unable to open " + cmd);
        }
      } else {
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
    if (restart) {
      thread_.sleep_for(std::chrono::seconds(config_["restart-interval"].asUInt()));
    }
  };
}

void waybar::modules::Custom::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

bool waybar::modules::Custom::handleScroll(GdkEventScroll* e) {
  auto ret = ALabel::handleScroll(e);
  thread_.wake_up();
  return ret;
}

bool waybar::modules::Custom::handleToggle(GdkEventButton* const& e) {
  auto ret = ALabel::handleToggle(e);
  thread_.wake_up();
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
          label_.set_tooltip_text(str);
        } else {
          label_.set_tooltip_text(tooltip_);
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

void waybar::modules::Custom::parseOutputRaw() {
  std::istringstream output(output_.out);
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

void waybar::modules::Custom::parseOutputJson() {
  std::istringstream output(output_.out);
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
