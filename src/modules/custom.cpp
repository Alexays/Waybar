#include "modules/custom.hpp"

waybar::modules::Custom::Custom(const std::string& name,
  const Json::Value& config)
  : ALabel(config, "{}"), name_(name), fp_(nullptr)
{
  label_.set_name("custom-" + name_);
  if (config_["exec"].isString()) {
    if (interval_.count() > 0) {
      delayWorker();
    } else {
      continuousWorker();
    }
  }
  dp.emit();
}

waybar::modules::Custom::~Custom()
{
  if (fp_) {
    pclose(fp_);
    fp_ = nullptr;
  }
}

void waybar::modules::Custom::delayWorker()
{
  thread_ = [this] {
    bool can_update = true;
    if (config_["exec-if"].isString()) {
      auto res = waybar::util::command::exec(config_["exec-if"].asString());
      if (res.exit_code != 0) {
        can_update = false;
        event_box_.hide();
      }
    }
    if (can_update) {
      output_ = waybar::util::command::exec(config_["exec"].asString());
      dp.emit();
    }
    thread_.sleep_for(interval_);
  };
}

void waybar::modules::Custom::continuousWorker()
{
  auto cmd = config_["exec"].asString();
  fp_ = popen(cmd.c_str(), "r");
  if (!fp_) {
    throw std::runtime_error("Unable to open " + cmd);
  }
  thread_ = [this] {
    char* buff = nullptr;
    size_t len = 0;
    if (getline(&buff, &len, fp_) == -1) {
      int exit_code = 1;
      if (fp_) {
        exit_code = WEXITSTATUS(pclose(fp_));
        fp_ = nullptr;
      }
      thread_.stop();
      if (exit_code != 0) {
        output_ = { exit_code, "" };
        dp.emit();
        std::cerr << name_ + " just stopped unexpectedly, is it endless?" << std::endl;
      }
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
  if (config_["exec"].isString() && (output_.out.empty() || output_.exit_code != 0)) {
    event_box_.hide();
  } else {
    if (config_["return-type"].asString() == "json") {
      parseOutputJson();
    } else {
      parseOutputRaw();
    }

    auto str = fmt::format(format_, text_,
      fmt::arg("alt", alt_),
      fmt::arg("icon", getIcon(percentage_)),
      fmt::arg("percentage", percentage_));
    label_.set_markup(str);
    if (tooltipEnabled()) {
      if (text_ == tooltip_) {
        label_.set_tooltip_text(str);
      } else {
        label_.set_tooltip_text(tooltip_);
      }
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

    event_box_.show();
  }
}

void waybar::modules::Custom::parseOutputRaw()
{
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
    class_ = parsed["class"].asString();
    if (!parsed["percentage"].asString().empty() && parsed["percentage"].isUInt()) {
      percentage_ = parsed["percentage"].asUInt();
    } else {
      percentage_ = 0;
    }
    break;
  }
}
