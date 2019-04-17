#pragma once

#include <thread>
#include <fmt/format.h>
#include <mpd/client.h>
#include "ALabel.hpp"

namespace waybar::modules {

class MPD : public ALabel {
  public:
    MPD(const std::string&, const Json::Value&);
    auto update() -> void;
  private:
    std::thread worker();
    void setLabel();
    std::string getStateIcon();

    void tryConnect();
    void checkErrors();

    void fetchState();
    void waitForEvent();

    std::thread worker_;

    using unique_connection = std::unique_ptr<mpd_connection, decltype(&mpd_connection_free)>;
    using unique_status     = std::unique_ptr<mpd_status, decltype(&mpd_status_free)>;
    using unique_song      = std::unique_ptr<mpd_song, decltype(&mpd_song_free)>;

    // Not using unique_ptr since we don't manage the pointer
    // (It's either nullptr, or from the config)
    const char* server_;
    const unsigned port_;

    unique_connection connection_;
    unique_status     status_;
    mpd_state         state_;
    unique_song       song_;

    bool stopped_;
};

}  // namespace waybar::modules
