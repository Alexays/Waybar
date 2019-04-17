#include "modules/mpd.hpp"

#include <iostream>

waybar::modules::MPD::MPD(const std::string& id, const Json::Value &config)
    : ALabel(config, "{album} - {artist} - {title}", 5),
      server_(nullptr),
      port_(config["port"].asUInt()),
      connection_(nullptr, &mpd_connection_free),
      alternate_connection_(nullptr, &mpd_connection_free),
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

  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &MPD::handlePlayPause));
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

  std::string stateIcon = "";
  if (stopped_) {
    format = config_["format-stopped"].isString() ?
      config_["format-stopped"].asString() : "stopped";
    label_.get_style_context()->add_class("stopped");
  } else {
    label_.get_style_context()->remove_class("stopped");

    stateIcon = getStateIcon();

    artist       = mpd_song_get_tag(song_.get(), MPD_TAG_ARTIST, 0);
    album_artist = mpd_song_get_tag(song_.get(), MPD_TAG_ALBUM_ARTIST, 0);
    album        = mpd_song_get_tag(song_.get(), MPD_TAG_ALBUM, 0);
    title        = mpd_song_get_tag(song_.get(), MPD_TAG_TITLE, 0);
    date         = mpd_song_get_tag(song_.get(), MPD_TAG_DATE, 0);
  }

  bool consumeActivated = mpd_status_get_consume(status_.get());
  std::string consumeIcon = getOptionIcon("consume", consumeActivated);
  bool randomActivated = mpd_status_get_random(status_.get());
  std::string randomIcon = getOptionIcon("random", randomActivated);
  bool repeatActivated = mpd_status_get_repeat(status_.get());
  std::string repeatIcon = getOptionIcon("repeat", repeatActivated);
  bool singleActivated = mpd_status_get_single(status_.get());
  std::string singleIcon = getOptionIcon("single", singleActivated);

  // TODO: format can fail
  label_.set_markup(fmt::format(format,
        fmt::arg("artist", artist),
        fmt::arg("albumArtist", album_artist),
        fmt::arg("album", album),
        fmt::arg("title", title),
        fmt::arg("date", date),
        fmt::arg("stateIcon", stateIcon),
        fmt::arg("consumeIcon", consumeIcon),
        fmt::arg("randomIcon", randomIcon),
        fmt::arg("repeatIcon", repeatIcon),
        fmt::arg("singleIcon", singleIcon)));

  if (tooltipEnabled()) {
    std::string tooltip_format;
    tooltip_format = config_["tooltip-format"].isString() ?
      config_["tooltip-format"].asString() : "MPD (connected)";
    auto tooltip_text = fmt::format(tooltip_format,
        fmt::arg("artist", artist),
        fmt::arg("albumArtist", album_artist),
        fmt::arg("album", album),
        fmt::arg("title", title),
        fmt::arg("date", date),
        fmt::arg("stateIcon", stateIcon),
        fmt::arg("consumeIcon", consumeIcon),
        fmt::arg("randomIcon", randomIcon),
        fmt::arg("repeatIcon", repeatIcon),
        fmt::arg("singleIcon", singleIcon));
    label_.set_tooltip_text(tooltip_text);
  }
}

std::string waybar::modules::MPD::getStateIcon() {
  if (!config_["state-icons"].isObject()) {
    return "";
  }

  if (connection_ == nullptr) {
    std::cerr << "MPD: Trying to fetch state icon while disconnected" << std::endl;
    return "";
  }

  if (stopped_) {
    std::cerr << "MPD: Trying to fetch state icon while stopped" << std::endl;
    return "";
  }

  if (state_ == MPD_STATE_PLAY) {
    return config_["state-icons"]["playing"].asString();
  } else {
    // MPD_STATE_PAUSE
    return config_["state-icons"]["paused"].asString();
  }
}

std::string waybar::modules::MPD::getOptionIcon(std::string optionName, bool activated) {
  if (!config_[optionName + "-icons"].isObject()) {
    return "";
  }

  if (connection_ == nullptr) {
    std::cerr << "MPD: Trying to fetch option icon while disconnected" << std::endl;
    return "";
  }

  if (activated) {
    return config_[optionName + "-icons"]["on"].asString();
  } else {
    return config_[optionName + "-icons"]["off"].asString();
  }
}

void waybar::modules::MPD::tryConnect() {
  if (connection_ != nullptr) {
    return;
  }

  connection_ = unique_connection(
      mpd_connection_new(server_, port_, 5'000),
      &mpd_connection_free);

  alternate_connection_ = unique_connection(
      mpd_connection_new(server_, port_, 5'000),
      &mpd_connection_free);

  if (connection_ == nullptr || alternate_connection_ == nullptr) {
    std::cerr << "Failed to connect to MPD" << std::endl;
    connection_.reset();
    alternate_connection_.reset();
    return;
  }

  try {
    checkErrors();
  } catch (std::runtime_error e) {
    std::cerr << "Failed to connect to MPD: " << e.what() << std::endl;
    connection_.reset();
    alternate_connection_.reset();
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
      alternate_connection_.reset();
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
    state_ = mpd_status_get_state(status_.get());
    checkErrors();
    stopped_ = state_ == MPD_STATE_UNKNOWN || state_ == MPD_STATE_STOP;

    song_ = unique_song(mpd_run_current_song(connection_.get()), &mpd_song_free);
    checkErrors();
}

void waybar::modules::MPD::waitForEvent() {
  auto conn = connection_.get();
  // Wait for a player (play/pause), option (random, shuffle, etc.), or playlist change
  mpd_run_idle_mask(conn, static_cast<mpd_idle>(MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS | MPD_IDLE_PLAYLIST));
  checkErrors();
}

bool waybar::modules::MPD::handlePlayPause(GdkEventButton* const& e) {
  if (e->type == GDK_2BUTTON_PRESS || e->type == GDK_3BUTTON_PRESS || alternate_connection_ == nullptr) {
    return false;
  }

  if (e->button == 1) {
    if (stopped_) {
      mpd_run_play(alternate_connection_.get());
    } else {
      mpd_run_toggle_pause(alternate_connection_.get());
    }
  } else if (e->button == 3) {
    mpd_run_stop(alternate_connection_.get());
  }

  return true;
}
