#include "modules/custom.hpp"

waybar::modules::Custom::Custom(const std::string name,
  const Json::Value& config)
  : ALabel(config, "{}"), name_(name)
{
  if (!config_["exec"]) {
    throw std::runtime_error(name_ + " has no exec path.");
  }
  if (config_["interval"]) {
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
    if (config_["exec-if"]) {
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
    auto str = fmt::format(format_, output_.out);
    label_.set_text(str);
    label_.set_tooltip_text(str);
    label_.show();
  }
}