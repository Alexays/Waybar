#include "modules/battery.hpp"

waybar::modules::Battery::Battery(Json::Value config)
  : ALabel(std::move(config))
{
  try {
    for (auto &node : fs::directory_iterator(data_dir_)) {
      if (fs::is_directory(node) && fs::exists(node / "capacity")
        && fs::exists(node / "status") && fs::exists(node / "uevent")) {
        batteries_.push_back(node);
      }
    }
  } catch (fs::filesystem_error &e) {
    throw std::runtime_error(e.what());
  }
  if (batteries_.empty()) {
    throw std::runtime_error("No batteries.");
  }
  auto fd = inotify_init1(IN_CLOEXEC);
  if (fd == -1) {
    throw std::runtime_error("Unable to listen batteries.");
  }
  for (auto &bat : batteries_) {
    inotify_add_watch(fd, (bat / "uevent").c_str(), IN_ACCESS);
  }
  // Trigger first value
  Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Battery::update));
  label_.set_name("battery");
  thread_ = [this, fd] {
    struct inotify_event event = {0};
    int nbytes = read(fd, &event, sizeof(event));
    if (nbytes != sizeof(event)) {
      return;
    }
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Battery::update));
  };
}

auto waybar::modules::Battery::update() -> void
{
  try {
    uint16_t total = 0;
    std::string status;
    for (auto &bat : batteries_) {
      uint16_t capacity;
      std::string _status;
      std::ifstream(bat / "capacity") >> capacity;
      std::ifstream(bat / "status") >> _status;
      if (_status != "Unknown") {
        status = _status;
      }
      total += capacity;
    }
    uint16_t capacity = total / batteries_.size();
    auto format = config_["format"]
      ? config_["format"].asString() : "{capacity}%";
    label_.set_text(fmt::format(format, fmt::arg("capacity", capacity),
      fmt::arg("icon", getIcon(capacity))));
    label_.set_tooltip_text(status);
    bool charging = status == "Charging";
    if (charging) {
      label_.get_style_context()->add_class("charging");
    } else {
      label_.get_style_context()->remove_class("charging");
    }
    auto critical = config_["critical"] ? config_["critical"].asUInt() : 15;
    if (capacity <= critical && !charging) {
      label_.get_style_context()->add_class("warning");
    } else {
      label_.get_style_context()->remove_class("warning");
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

std::string waybar::modules::Battery::getIcon(uint16_t percentage)
{
  if (!config_["format-icons"] || !config_["format-icons"].isArray()) {
    return "";
  }
  auto size = config_["format-icons"].size();
  auto idx = std::clamp(percentage / (100 / size), 0U, size - 1);
  return config_["format-icons"][idx].asString();
}
