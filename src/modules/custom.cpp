#include "modules/custom.hpp"

#include <spdlog/spdlog.h>

namespace waybar::modules {

Custom::Custom(const std::string& name, const std::string& id, const Json::Value& config)
    : ALabel(config, "custom-" + name, id, "{}", "{tooltip}"), name_(name), fp_(nullptr), pid_(-1) {
  // Save context in order to restore it later
  label_.get_style_context()->context_save();

  // Hide box by default
  event_box_.hide();

  if (interval_.count() > 0) {
    delayWorker();
  } else if (config_["exec"].isString()) {
    continuousWorker();
  } else {
    dp.emit();
  }
}

Custom::~Custom() {
  if (pid_ != -1) {
    killpg(pid_, SIGTERM);
    pid_ = -1;
  }
}

void Custom::delayWorker() {
  thread_ = [this] {
    bool can_update = true;
    if (config_["exec-if"].isString()) {
      output_ = util::command::execNoRead(config_["exec-if"].asString());
      if (output_.exit_code != 0) {
        can_update = false;
        event_box_.hide();
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

void Custom::continuousWorker() {
  auto cmd = config_["exec"].asString();
  pid_ = -1;
  fp_ = util::command::open(cmd, pid_);
  if (!fp_) {
    throw std::runtime_error("Unable to open " + cmd);
  }
  thread_ = [&] {
    char* buff = nullptr;
    size_t len = 0;
    bool restart = false;
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

void Custom::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

bool Custom::handleScroll(GdkEventScroll* e) {
  auto ret = ALabel::handleScroll(e);
  thread_.wake_up();
  return ret;
}

bool Custom::handleToggle(GdkEventButton* const& e) {
  auto ret = ALabel::handleToggle(e);
  thread_.wake_up();
  return ret;
}

auto Custom::update(std::string format, waybar::args& args) -> void {
  // Hide label if output is empty
  if (config_["exec"].isString() && (output_.out.empty() || output_.exit_code != 0)) {
    event_box_.hide();
    return;
  }

  // Restore default context
  label_.get_style_context()->context_restore();

  if (config_["return-type"].asString() == "json") {
    parseOutputJson();
  } else {
    parseOutputRaw();
  }

  args.push_back(text_);
  auto altArg = fmt::arg("alt", alt_);
  args.push_back(std::cref(altArg));
  auto icon = getIcon(percentage_, alt_);
  auto iconArg = fmt::arg("icon", icon);
  args.push_back(std::cref(iconArg));
  auto percentageArg = fmt::arg("percentage", percentage_);
  args.push_back(std::cref(percentageArg));

  if (tooltip_.empty()) {
    auto tooltipArg = fmt::arg("tooltip", text_);
    args.push_back(std::cref(tooltipArg));
  } else {
    auto tooltipArg = fmt::arg("tooltip", tooltip_);
    args.push_back(std::cref(tooltipArg));
  }

  // Add classes
  for (auto const& c : class_) {
    label_.get_style_context()->add_class(c);
  }

  // Call parent update
  ALabel::update(format, args);
}

void Custom::parseOutputRaw() {
  std::istringstream output(output_.out);
  std::string line;
  int i = 0;
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

void Custom::parseOutputJson() {
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

}  // namespace waybar::modules
