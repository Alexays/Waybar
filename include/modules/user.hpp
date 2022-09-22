#pragma once

#include <fmt/chrono.h>

#include "AIconLabel.hpp"
#include "util/sleeper_thread.hpp"
#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>

namespace waybar::modules
{
    class User: public AIconLabel
    {
        public:
            User(const std::string&, const Json::Value&);
            ~User() = default;
            auto update() -> void;

        private:
            util::SleeperThread thread_;

            Glib::RefPtr<Gdk::Pixbuf> pixbuf_;

            static constexpr inline int defaultUserImageWidth_ = 20;
            static constexpr inline int defaultUserImageHeight_ = 20;

            long uptime_as_seconds();
            std::string get_user_login();
            std::string get_user_home_dir();
            std::string get_default_user_avatar_path();
            void init_default_user_avatar(int width, int height);
            void init_user_avatar(const std::string& path, int width, int height);
            void init_avatar(const Json::Value& config);
            void init_update_worker();
    };
}  // namespace waybar::modules
