#include "modules/battery.hpp"

waybar::modules::Battery::Battery(const Json::Value& config)
  : ALabel(config, "{capacity}%")
{
  try {
    if (config_["bat"].isString()) {
      auto dir = data_dir_ / config_["bat"].asString();
      if (fs::is_directory(dir) && fs::exists(dir / "capacity")
        && fs::exists(dir / "status") && fs::exists(dir / "uevent")) {
        batteries_.push_back(dir);
      }
    } else {
      for (auto const& node : fs::directory_iterator(data_dir_)) {
        if (fs::is_directory(node) && fs::exists(node / "capacity")
          && fs::exists(node / "status") && fs::exists(node / "uevent")) {
          batteries_.push_back(node);
        }
      }
    }
  } catch (fs::filesystem_error &e) {
    throw std::runtime_error(e.what());
  }
  if (batteries_.empty()) {
    if (config_["bat"].isString()) {
      throw std::runtime_error("No battery named " + config_["bat"].asString());
    }
    throw std::runtime_error("No batteries.");
  }
  fd_ = inotify_init1(IN_CLOEXEC);
  if (fd_ == -1) {
    throw std::runtime_error("Unable to listen batteries.");
  }
  for (auto const& bat : batteries_) {
    inotify_add_watch(fd_, (bat / "uevent").c_str(), IN_ACCESS);
  }
  worker();
}

waybar::modules::Battery::~Battery()
{
  close(fd_);
}

void waybar::modules::Battery::worker()
{
  // Trigger first values
  update();
  uint32_t interval = config_["interval"].isUInt() ? config_["interval"].asUInt() : 60;
  thread_timer_ = [this, interval] {
    thread_.sleep_for(chrono::seconds(interval));
    dp.emit();
  };
  thread_ = [this] {
    struct inotify_event event = {0};
    int nbytes = read(fd_, &event, sizeof(event));
    if (nbytes != sizeof(event)) {
      return;
    }
    // TODO: don't stop timer for now since there is some bugs :?
    // thread_timer_.stop();
    dp.emit();
  };
}

std::tuple<uint16_t, std::string> waybar::modules::Battery::getInfos()
{
  try {
    uint16_t total = 0;
    std::string status = "Unknown";
    for (auto const& bat : batteries_) {
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
    return {capacity, status};
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return {0, "Unknown"};
  }
}

std::string waybar::modules::Battery::getState(uint16_t capacity)
{
  // Get current state
  std::vector<std::pair<std::string, uint16_t>> states;
  if (config_["states"].isObject()) {
    for (auto it = config_["states"].begin(); it != config_["states"].end(); ++it) {
      if (it->isUInt() && it.key().isString()) {
        states.push_back({it.key().asString(), it->asUInt()});
      }
    }
  }
  // Sort states
  std::sort(states.begin(), states.end(), [](auto &a, auto &b) {
    return a.second < b.second;
  });
  std::string validState = "";
  for (auto state : states) {
    if (capacity <= state.second && validState.empty()) {
      label_.get_style_context()->add_class(state.first);
      validState = state.first;
    } else {
      label_.get_style_context()->remove_class(state.first);
    }
  }
  return validState;
}

auto waybar::modules::Battery::update() -> void
{
  auto [capacity, status] = getInfos();
  label_.set_tooltip_text(status);
  std::transform(status.begin(), status.end(), status.begin(), ::tolower);
  auto format = format_;
  auto state = getState(capacity);
  label_.get_style_context()->remove_class(old_status_);
  label_.get_style_context()->add_class(status);
  old_status_ = status;
  if (!state.empty() && config_["format-" + status + "-" + state].isString()) {
    format = config_["format-" + status + "-" + state].asString();
  } else if (config_["format-" + status].isString()) {
    format = config_["format-" + status].asString();
  } else if (!state.empty() && config_["format-" + state].isString()) {
    format = config_["format-" + state].asString();
  }
  if (format.empty()) {
    event_box_.hide();
    label_.set_name("");
  } else {
    event_box_.show();
    label_.set_name("battery");
    label_.set_text(fmt::format(format, fmt::arg("capacity", capacity),
      fmt::arg("icon", getIcon(capacity))));
  }
}
