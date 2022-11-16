#include "modules/user.hpp"

#include <fmt/chrono.h>
#include <glibmm/miscutils.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>

#include "gdkmm/cursor.h"
#include "gdkmm/event.h"
#include "gdkmm/types.h"
#include "sigc++/functors/mem_fun.h"
#include "sigc++/functors/ptr_fun.h"

#if HAVE_CPU_LINUX
#include <sys/sysinfo.h>
#endif

#if HAVE_CPU_BSD
#include <time.h>
#endif

const static int LEFT_MOUSE_BUTTON_CODE = 1;

namespace waybar::modules {
User::User(const std::string& id, const Json::Value& config)
    : AIconLabel(config, "user", id, "{user} {work_H}:{work_M}", 60, false, true, true) {
  if (AIconLabel::iconEnabled()) {
    this->init_avatar(AIconLabel::config_);
  }
  this->init_update_worker();
}

bool User::handleToggle(GdkEventButton* const& e) {
  if (AIconLabel::config_["open-on-click"].isBool() &&
      AIconLabel::config_["open-on-click"].asBool() && e->button == LEFT_MOUSE_BUTTON_CODE) {
    std::string openPath = this->get_user_home_dir();
    if (AIconLabel::config_["open-path"].isString()) {
      std::string customPath = AIconLabel::config_["open-path"].asString();
      if (!customPath.empty()) {
        openPath = std::move(customPath);
      }
    }

    Gio::AppInfo::launch_default_for_uri("file:///" + openPath);
  }
  return true;
}

long User::uptime_as_seconds() {
  long uptime = 0;

#if HAVE_CPU_LINUX
  struct sysinfo s_info;
  if (0 == sysinfo(&s_info)) {
    uptime = s_info.uptime;
  }
#endif

#if HAVE_CPU_BSD
  struct timespec s_info;
  if (0 == clock_gettime(CLOCK_UPTIME_PRECISE, &s_info)) {
    uptime = s_info.tv_sec;
  }
#endif

  return uptime;
}

std::string User::get_user_login() const { return Glib::get_user_name(); }

std::string User::get_user_home_dir() const { return Glib::get_home_dir(); }

void User::init_update_worker() {
  this->thread_ = [this] {
    ALabel::dp.emit();
    auto now = std::chrono::system_clock::now();
    auto diff = now.time_since_epoch() % ALabel::interval_;
    this->thread_.sleep_for(ALabel::interval_ - diff);
  };
}

void User::init_avatar(const Json::Value& config) {
  int height =
      config["height"].isUInt() ? config["height"].asUInt() : this->defaultUserImageHeight_;
  int width = config["width"].isUInt() ? config["width"].asUInt() : this->defaultUserImageWidth_;

  if (config["avatar"].isString()) {
    std::string userAvatar = config["avatar"].asString();
    if (!userAvatar.empty()) {
      this->init_user_avatar(userAvatar, width, height);
      return;
    }
  }

  this->init_default_user_avatar(width, width);
}

std::string User::get_default_user_avatar_path() const {
  return this->get_user_home_dir() + "/" + ".face";
}

void User::init_default_user_avatar(int width, int height) {
  this->init_user_avatar(this->get_default_user_avatar_path(), width, height);
}

void User::init_user_avatar(const std::string& path, int width, int height) {
  Glib::RefPtr<Gdk::Pixbuf> pixbuf_ = Gdk::Pixbuf::create_from_file(path, width, height);
  AIconLabel::image_.set(pixbuf_);
}

auto User::update() -> void {
  std::string systemUser = this->get_user_login();
  std::transform(systemUser.cbegin(), systemUser.cend(), systemUser.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  long uptimeSeconds = this->uptime_as_seconds();
  auto workSystemTimeSeconds = std::chrono::seconds(uptimeSeconds);
  auto currentSystemTime = std::chrono::system_clock::now();
  auto startSystemTime = currentSystemTime - workSystemTimeSeconds;
  long workSystemDays = uptimeSeconds / 86400;

  auto label = fmt::format(ALabel::format_, fmt::arg("up_H", fmt::format("{:%H}", startSystemTime)),
                           fmt::arg("up_M", fmt::format("{:%M}", startSystemTime)),
                           fmt::arg("up_d", fmt::format("{:%d}", startSystemTime)),
                           fmt::arg("up_m", fmt::format("{:%m}", startSystemTime)),
                           fmt::arg("up_Y", fmt::format("{:%Y}", startSystemTime)),
                           fmt::arg("work_d", workSystemDays),
                           fmt::arg("work_H", fmt::format("{:%H}", workSystemTimeSeconds)),
                           fmt::arg("work_M", fmt::format("{:%M}", workSystemTimeSeconds)),
                           fmt::arg("work_S", fmt::format("{:%S}", workSystemTimeSeconds)),
                           fmt::arg("user", systemUser));
  ALabel::label_.set_markup(label);
  ALabel::update();
}
};  // namespace waybar::modules
