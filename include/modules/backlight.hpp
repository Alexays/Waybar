#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ALabel.hpp"
#include "giomm/dbusproxy.h"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

struct udev;
struct udev_device;

namespace waybar::modules {

class Backlight : public ALabel {
  class BacklightDev {
   public:
    BacklightDev() = default;
    BacklightDev(std::string name, int actual, int max, bool powered);
    std::string_view name() const;
    int get_actual() const;
    void set_actual(int actual);
    int get_max() const;
    void set_max(int max);
    bool get_powered() const;
    void set_powered(bool powered);
    friend inline bool operator==(const BacklightDev &lhs, const BacklightDev &rhs) {
      return lhs.name_ == rhs.name_ && lhs.actual_ == rhs.actual_ && lhs.max_ == rhs.max_;
    }

   private:
    std::string name_;
    int actual_ = 1;
    int max_ = 1;
    bool powered_ = true;
  };

 public:
  Backlight(const std::string &, const Json::Value &);
  virtual ~Backlight();
  auto update() -> void override;

 private:
  template <class ForwardIt>
  static const BacklightDev *best_device(ForwardIt first, ForwardIt last, std::string_view);
  template <class ForwardIt, class Inserter>
  static void upsert_device(ForwardIt first, ForwardIt last, Inserter inserter, udev_device *dev);
  template <class ForwardIt, class Inserter>
  static void enumerate_devices(ForwardIt first, ForwardIt last, Inserter inserter, udev *udev);

  bool handleScroll(GdkEventScroll *e) override;

  const std::string preferred_device_;
  static constexpr int EPOLL_MAX_EVENTS = 16;

  std::optional<BacklightDev> previous_best_;
  std::string previous_format_;

  std::mutex udev_thread_mutex_;
  std::vector<BacklightDev> devices_;
  // thread must destruct before shared data
  util::SleeperThread udev_thread_;

  Glib::RefPtr<Gio::DBus::Proxy> login_proxy_;
};
}  // namespace waybar::modules
