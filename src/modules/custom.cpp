#include "modules/custom.hpp"

waybar::modules::Custom::Custom(std::string name, Json::Value config)
  : ALabel(std::move(config)), name_(std::move(name))
{
  if (!config_["exec"]) {
    throw std::runtime_error(name_ + " has no exec path.");
  }
  uint32_t interval = config_["interval"] ? config_["inveral"].asUInt() : 30;
  thread_ = [this, interval] {
    bool can_update = true;
    if (config_["exec-if"]) {
      auto pid = fork();
      int res = 0;
      if (pid == 0) {
        std::istringstream iss(config_["exec-if"].asString());
        std::vector<char*> av;
        for (std::string s; iss >> s;) {
          // Need to copy otherwise values are the same
          char *str = new char[s.size() + 1];
          memcpy(str, s.c_str(), s.size() + 1);
          av.push_back(str);
        }
        av.push_back(0);
        execvp(av.front(), av.data());
        _exit(127);
      } else if (pid > 0 && waitpid(pid, &res, 0) != -1
        && WEXITSTATUS(res) != 0) {
        can_update = false;
      }
    }
    if (can_update) {
      Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Custom::update));
    }
    thread_.sleep_for(chrono::seconds(interval));
  };
}

auto waybar::modules::Custom::update() -> void
{
  std::array<char, 128> buffer = {0};
  std::string output;
  std::shared_ptr<FILE> fp(popen(config_["exec"].asCString(), "r"), pclose);
  if (!fp) {
    std::cerr << name_ + " can't exec " + config_["exec"].asString() << std::endl;
    return;
  }

  while (feof(fp.get()) == 0) {
    if (fgets(buffer.data(), 128, fp.get()) != nullptr) {
        output += buffer.data();
    }
  }

  // Remove last newline
  if (!output.empty() && output[output.length()-1] == '\n') {
    output.erase(output.length()-1);
  }

  // Hide label if output is empty
  if (output.empty()) {
    label_.hide();
    label_.set_name("");
  } else {
    label_.set_name("custom-" + name_);
    auto format = config_["format"] ? config_["format"].asString() : "{}";
    auto str = fmt::format(format, output);
    label_.set_text(str);
    label_.set_tooltip_text(str);
    label_.show();
  }
}
