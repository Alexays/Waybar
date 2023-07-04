#pragma once

#include <fmt/chrono.h>
#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

#include "AIconLabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {
class User : public AIconLabel {
 public:
  User(const std::string&, const Json::Value&);
  virtual ~User() = default;
  auto update() -> void override;

  bool handleToggle(GdkEventButton* const& e) override;

 private:
  util::SleeperThread thread_;

  static constexpr inline int defaultUserImageWidth_ = 20;
  static constexpr inline int defaultUserImageHeight_ = 20;

  long uptime_as_seconds();
  std::string get_user_login() const;
  std::string get_user_home_dir() const;
  std::string get_default_user_avatar_path() const;
  void init_default_user_avatar(int width, int height);
  void init_user_avatar(const std::string& path, int width, int height);
  void init_avatar(const Json::Value& config);
  void init_update_worker();
};
}  // namespace waybar::modules
