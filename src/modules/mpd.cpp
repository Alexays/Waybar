#include "modules/mpd.hpp"

#include <iostream>

waybar::modules::MPD::MPD(const std::string& id, const Json::Value &config)
    : ALabel(config, "{album} - {artist} - {title}", 5),
      server_(nullptr),
      port_(config["port"].asUInt()),
      connection_(nullptr, &mpd_connection_free),
      status_(nullptr, &mpd_status_free),
      song_(nullptr, &mpd_song_free) {
  label_.set_name("mpd");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }

  if (!config["server"].isNull()) {
    server_ = config["server"].asCString();
  }

  worker_ = worker();
}

auto waybar::modules::MPD::update() -> void {
  tryConnect();

  if (connection_ != nullptr) {
    try {
      fetchState();
    } catch (std::exception e) {
      stopped_ = true;
    }
  }

  setLabel();
}

std::thread waybar::modules::MPD::worker() {
  return std::thread([this] () {
      while (true) {
        if (connection_ == nullptr) {
          // Retry periodically if no connection
          update();
          std::this_thread::sleep_for(interval_);
        } else {
          // Else, update on any event
          waitForEvent();
          update();
        }
      }
  });
}

void waybar::modules::MPD::setLabel() {
  if (connection_ == nullptr) {
    label_.get_style_context()->add_class("disconnected");
    // In the case connection closed while MPD is stopped
    label_.get_style_context()->remove_class("stopped");

    auto format = config_["format-disconnected"].isString() ?
      config_["format-disconnected"].asString() : "disconnected";
    label_.set_markup(format);

    if (tooltipEnabled()) {
      std::string tooltip_format;
      tooltip_format = config_["tooltip-format-disconnected"].isString() ?
        config_["tooltip-format-disconnected"].asString() : "MPD (disconnected)";
      // Nothing to format
      label_.set_tooltip_text(tooltip_format);
    }
    return;
  } else {
    label_.get_style_context()->remove_class("disconnected");
  }

  auto format = format_;

  std::string artist, album_artist, album, title, date;

  if (stopped_) {
    format = config_["format-stopped"].isString() ?
      config_["format-stopped"].asString() : "stopped";
    label_.get_style_context()->add_class("stopped");
  } else {
    label_.get_style_context()->remove_class("stopped");

    artist       = mpd_song_get_tag(song_.get(), MPD_TAG_ARTIST, 0);
    album_artist = mpd_song_get_tag(song_.get(), MPD_TAG_ALBUM_ARTIST, 0);
    album        = mpd_song_get_tag(song_.get(), MPD_TAG_ALBUM, 0);
    title        = mpd_song_get_tag(song_.get(), MPD_TAG_TITLE, 0);
    date         = mpd_song_get_tag(song_.get(), MPD_TAG_DATE, 0);
  }

  label_.set_markup(fmt::format(format,
        fmt::arg("artist", artist),
        fmt::arg("album-artist", album_artist),
        fmt::arg("album", album),
        fmt::arg("title", title),
        fmt::arg("date", date)));

  if (tooltipEnabled()) {
    std::string tooltip_format;
    tooltip_format = config_["tooltip-format"].isString() ?
      config_["tooltip-format"].asString() : "MPD (connected)";
    auto tooltip_text = fmt::format(tooltip_format,
        fmt::arg("artist", artist),
        fmt::arg("album-artist", album_artist),
        fmt::arg("album", album),
        fmt::arg("title", title),
        fmt::arg("date", date));
    label_.set_tooltip_text(tooltip_text);
  }
}

void waybar::modules::MPD::tryConnect() {
  if (connection_ != nullptr) {
    return;
  }

  connection_ = unique_connection(
      mpd_connection_new(server_, port_, 5'000),
      &mpd_connection_free);

  if (connection_ == nullptr) {
    std::cerr << "Failed to connect to MPD" << std::endl;
    return;
  }

  try {
    checkErrors();
  } catch (std::runtime_error e) {
    std::cerr << "Failed to connect to MPD: " << e.what() << std::endl;
    connection_.reset();
  }

}

void waybar::modules::MPD::checkErrors() {
  auto conn = connection_.get();

  switch (mpd_connection_get_error(conn)) {
    case MPD_ERROR_SUCCESS:
      return;
    case MPD_ERROR_CLOSED:
      std::cerr << "Connection to MPD closed" << std::endl;
      mpd_connection_clear_error(conn);
      connection_.reset();
      return;
    default:
      auto error_message = mpd_connection_get_error_message(conn);
      mpd_connection_clear_error(conn);
      throw std::runtime_error(std::string(error_message));
  }
}

void waybar::modules::MPD::fetchState() {
    status_ = unique_status(mpd_run_status(connection_.get()), &mpd_status_free);
    checkErrors();
    mpd_state state = mpd_status_get_state(status_.get());
    checkErrors();
    stopped_ = state == MPD_STATE_UNKNOWN || state == MPD_STATE_STOP;

    mpd_send_current_song(connection_.get());
    song_ = unique_song(mpd_recv_song(connection_.get()), &mpd_song_free);
    mpd_response_finish(connection_.get());
    checkErrors();
}

void waybar::modules::MPD::waitForEvent() {
  auto conn = connection_.get();
  mpd_run_idle_mask(conn, MPD_IDLE_PLAYER /* | MPD_IDLE_OPTIONS */);
  checkErrors();
}
