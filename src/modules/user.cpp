#include "modules/user.hpp"

#include <fmt/chrono.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <glibmm/miscutils.h>

namespace waybar::modules
{
    User::User(const std::string& id, const Json::Value& config)
        : AIconLabel(config, "info", id, "{user} {work_H}:{work_M}", 60, false, false, true)
    {
        this->init_avatar(AIconLabel::config_);
        this->init_update_worker();
    }

    long User::uptime_as_seconds()
    {
        struct sysinfo s_info;
        int error = sysinfo(&s_info);
        return s_info.uptime;
    }

    std::string User::get_user_login()
    {
        return Glib::get_user_name();
    }

    std::string User::get_user_home_dir()
    {
        return Glib::get_home_dir();
    }

    void User::init_update_worker()
    {
        this->thread_ = [this] {
            ALabel::dp.emit();
            auto now = std::chrono::system_clock::now();
            auto diff = now.time_since_epoch() % ALabel::interval_;
            this->thread_.sleep_for(ALabel::interval_ - diff);
        };
    }

    void User::init_avatar(const Json::Value& config)
    {
        int height = config["height"].isUInt() ? config["height"].asUInt() : this->defaultUserImageHeight_;
        int width = config["width"].isUInt() ? config["width"].asUInt() : this->defaultUserImageWidth_;
        
        if(config["avatar"].isString())
        {
            std::string userAvatar = config["avatar"].asString();
            if(!userAvatar.empty())
            {
                this->init_user_avatar(userAvatar, width, height);
                return;
            }
        }

        this->init_default_user_avatar(width, width);
    }

    std::string User::get_default_user_avatar_path()
    {
        return this->get_user_home_dir() + "/" + ".face";
    }

    void User::init_default_user_avatar(int width, int height)
    {
        this->init_user_avatar(this->get_default_user_avatar_path(), width, height);
    }

    void User::init_user_avatar(const std::string& path, int width, int height)
    {
        this->pixbuf_ = Gdk::Pixbuf::create_from_file(path, width, height);
        AIconLabel::image_.set(this->pixbuf_);
    }

    auto User::update() -> void
    {
        std::string systemUser = this->get_user_login();
        std::transform(systemUser.cbegin(), systemUser.cend(), systemUser.begin(), [](unsigned char c) { return std::toupper(c); });

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
};
