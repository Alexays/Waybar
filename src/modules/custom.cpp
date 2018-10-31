#include "modules/custom.hpp"

waybar::modules::Custom::Custom(const std::string name,
  const Json::Value& config)
  : ALabel(config, "{}"), name_(name)
{
  if (!config_["exec"].isString()) {
    throw std::runtime_error(name_ + " has no exec path.");
  }
  if (config_["interval"].isUInt()) {
    delayWorker();
  } else {
    continuousWorker();
  }
}

void waybar::modules::Custom::delayWorker()
{
  auto interval = config_["interval"].asUInt();
  thread_ = [this, interval] {
    bool can_update = true;
    if (config_["exec-if"].isString()) {
      auto res = waybar::util::command::exec(config_["exec-if"].asString());
      if (res.exit_code != 0) {
        can_update = false;
        label_.hide();
        label_.set_name("");
      }
    }
    if (can_update) {
      output_ = waybar::util::command::exec(config_["exec"].asString());
      dp.emit();
    }
    thread_.sleep_for(chrono::seconds(interval));
  };
}

void waybar::modules::Custom::continuousWorker()
{
  auto cmd = config_["exec"].asString();
  FILE* fp(popen(cmd.c_str(), "r"));
  if (!fp) {
    throw std::runtime_error("Unable to open " + cmd);
  }
  thread_ = [this, fp] {
    char* buff = nullptr;
    size_t len = 0;
    if (getline(&buff, &len, fp) == -1) {
      pclose(fp);
      thread_.stop();
      output_ = { 1, "" };
      dp.emit();
      return;
    }

    std::string output = buff;

    // Remove last newline
    if (!output.empty() && output[output.length()-1] == '\n') {
      output.erase(output.length()-1);
    }
    output_ = { 0, output };
    dp.emit();
  };
}

auto waybar::modules::Custom::update() -> void
{
  // Hide label if output is empty
  if (output_.out.empty() || output_.exit_code != 0) {
    label_.hide();
    label_.set_name("");
  } else {
    label_.set_name("custom-" + name_);

    if (config_["return-type"].asString() == "json") {
      parseOutputJson();
    } else {
      parseOutputRaw();
    }

    auto str = fmt::format(format_, text_);
    label_.set_text(str);
    if (text_ == tooltip_) {
      label_.set_tooltip_text(str);
    } else {
      label_.set_tooltip_text(tooltip_);
    }
    if (class_ != "") {
      if (prevclass_ != "") {
        label_.get_style_context()->remove_class(prevclass_);
      }
      label_.get_style_context()->add_class(class_);
      prevclass_ = class_;
    } else {
      label_.get_style_context()->remove_class(prevclass_);
      prevclass_ = "";
    }

    label_.show();
  }
}

void waybar::modules::Custom::parseOutputRaw()
{
  std::istringstream output(output_.out);
  std::string line;
  int i = 0;
  while (getline(output, line)) {
    if (i == 0) {
      text_ = line;
      tooltip_ = line;
      class_ = "";
    } else if (i == 1) {
      tooltip_ = line;
    } else if (i == 2) {
      class_ = line;
    } else {
      break;
    }
    i++;
  }
}

void waybar::modules::Custom::parseOutputJson()
{
  std::istringstream output(output_.out);
  std::string line;
  while (getline(output, line)) {
    auto parsed = parser_.parse(line);
    text_ = parsed["text"].asString();
    tooltip_ = parsed["tooltip"].asString();
    class_ = parsed["class"].asString();
    break;
  }
}