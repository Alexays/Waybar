#include "modules/mpd.hpp"

#include <fmt/chrono.h>
#include <iostream>

waybar::modules::MPD::MPD(const std::string& id, const Json::Value& config)
    : ALabel(config, "{album} - {artist} - {title}", 5),
      module_name_(id.empty() ? "mpd" : "mpd#" + id),
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

  event_listener().detach();

  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &MPD::handlePlayPause));
}

auto waybar::modules::MPD::update() -> void {
  std::lock_guard guard(connection_lock_);
  tryConnect();

  if (connection_ != nullptr) {
    try {
      bool wasPlaying = playing();
      fetchState();
      if (!wasPlaying && playing()) {
        periodic_updater().detach();
      }
    } catch (const std::exception& e) {
      std::cerr << module_name_ + ": " + e.what() << std::endl;
      state_ = MPD_STATE_UNKNOWN;
    }
  }

  setLabel();
}

std::thread waybar::modules::MPD::event_listener() {
  return std::thread([this] {
    while (true) {
      if (connection_ == nullptr) {
        // Retry periodically if no connection
        try {
          update();
        } catch (const std::exception& e) {
          std::cerr << module_name_ + ": " + e.what() << std::endl;
        }
        std::this_thread::sleep_for(interval_);
      } else {
        waitForEvent();
        dp.emit();
      }
    }
  });
}

std::thread waybar::modules::MPD::periodic_updater() {
  return std::thread([this] {
    while (connection_ != nullptr && playing()) {
      dp.emit();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });
}

void waybar::modules::MPD::setLabel() {
  if (connection_ == nullptr) {
    label_.get_style_context()->add_class("disconnected");
    // In the case connection closed while MPD is stopped
    label_.get_style_context()->remove_class("stopped");

    auto format = config_["format-disconnected"].isString()
                      ? config_["format-disconnected"].asString()
                      : "disconnected";
    label_.set_markup(format);

    if (tooltipEnabled()) {
      std::string tooltip_format;
      tooltip_format = config_["tooltip-format-disconnected"].isString()
                           ? config_["tooltip-format-disconnected"].asString()
                           : "MPD (disconnected)";
      // Nothing to format
      label_.set_tooltip_text(tooltip_format);
    }
    return;
  } else {
    label_.get_style_context()->remove_class("disconnected");
  }

  auto format = format_;

  std::string          artist, album_artist, album, title, date;
  std::chrono::seconds elapsedTime, totalTime;

  std::string stateIcon = "";
  if (stopped()) {
    format =
        config_["format-stopped"].isString() ? config_["format-stopped"].asString() : "stopped";
    label_.get_style_context()->add_class("stopped");
    label_.get_style_context()->remove_class("playing");
    label_.get_style_context()->remove_class("paused");
  } else {
    label_.get_style_context()->remove_class("stopped");
    if (playing()) {
      label_.get_style_context()->add_class("playing");
      label_.get_style_context()->remove_class("paused");
    } else {
      label_.get_style_context()->add_class("paused");
      label_.get_style_context()->remove_class("playing");
    }

    stateIcon = getStateIcon();

    artist = mpd_song_get_tag(song_.get(), MPD_TAG_ARTIST, 0);
    album_artist = mpd_song_get_tag(song_.get(), MPD_TAG_ALBUM_ARTIST, 0);
    album = mpd_song_get_tag(song_.get(), MPD_TAG_ALBUM, 0);
    title = mpd_song_get_tag(song_.get(), MPD_TAG_TITLE, 0);
    date = mpd_song_get_tag(song_.get(), MPD_TAG_DATE, 0);
    elapsedTime = std::chrono::seconds(mpd_status_get_elapsed_time(status_.get()));
    totalTime = std::chrono::seconds(mpd_status_get_total_time(status_.get()));
  }

  bool        consumeActivated = mpd_status_get_consume(status_.get());
  std::string consumeIcon = getOptionIcon("consume", consumeActivated);
  bool        randomActivated = mpd_status_get_random(status_.get());
  std::string randomIcon = getOptionIcon("random", randomActivated);
  bool        repeatActivated = mpd_status_get_repeat(status_.get());
  std::string repeatIcon = getOptionIcon("repeat", repeatActivated);
  bool        singleActivated = mpd_status_get_single(status_.get());
  std::string singleIcon = getOptionIcon("single", singleActivated);

  // TODO: format can fail
  label_.set_markup(fmt::format(format,
                                fmt::arg("artist", artist),
                                fmt::arg("albumArtist", album_artist),
                                fmt::arg("album", album),
                                fmt::arg("title", title),
                                fmt::arg("date", date),
                                fmt::arg("elapsedTime", elapsedTime),
                                fmt::arg("totalTime", totalTime),
                                fmt::arg("stateIcon", stateIcon),
                                fmt::arg("consumeIcon", consumeIcon),
                                fmt::arg("randomIcon", randomIcon),
                                fmt::arg("repeatIcon", repeatIcon),
                                fmt::arg("singleIcon", singleIcon)));

  if (tooltipEnabled()) {
    std::string tooltip_format;
    tooltip_format = config_["tooltip-format"].isString() ? config_["tooltip-format"].asString()
                                                          : "MPD (connected)";
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
    std::cerr << module_name_ << ": Trying to fetch state icon while disconnected" << std::endl;
    return "";
  }

  if (stopped()) {
    std::cerr << module_name_ << ": Trying to fetch state icon while stopped" << std::endl;
    return "";
  }

  if (playing()) {
    return config_["state-icons"]["playing"].asString();
  } else {
    return config_["state-icons"]["paused"].asString();
  }
}

std::string waybar::modules::MPD::getOptionIcon(std::string optionName, bool activated) {
  if (!config_[optionName + "-icons"].isObject()) {
    return "";
  }

  if (connection_ == nullptr) {
    std::cerr << module_name_ << ": Trying to fetch option icon while disconnected" << std::endl;
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

  connection_ = unique_connection(mpd_connection_new(server_, port_, 5'000), &mpd_connection_free);

  alternate_connection_ =
      unique_connection(mpd_connection_new(server_, port_, 5'000), &mpd_connection_free);

  if (connection_ == nullptr || alternate_connection_ == nullptr) {
    std::cerr << module_name_ << ": Failed to connect to MPD" << std::endl;
    connection_.reset();
    alternate_connection_.reset();
    return;
  }

  try {
    checkErrors(connection_.get());
  } catch (std::runtime_error e) {
    std::cerr << module_name_ << ": Failed to connect to MPD: " << e.what() << std::endl;
    connection_.reset();
    alternate_connection_.reset();
  }
}

void waybar::modules::MPD::checkErrors(mpd_connection* conn) {
  switch (mpd_connection_get_error(conn)) {
    case MPD_ERROR_SUCCESS:
      return;
    case MPD_ERROR_CLOSED:
      std::cerr << module_name_ << ": Connection to MPD closed" << std::endl;
      mpd_connection_clear_error(conn);
      connection_.reset();
      alternate_connection_.reset();
      state_ = MPD_STATE_UNKNOWN;
      return;
    default:
      auto error_message = mpd_connection_get_error_message(conn);
      mpd_connection_clear_error(conn);
      throw std::runtime_error(std::string(error_message));
  }
}

void waybar::modules::MPD::fetchState() {
  auto conn = connection_.get();
  status_ = unique_status(mpd_run_status(conn), &mpd_status_free);
  checkErrors(conn);
  state_ = mpd_status_get_state(status_.get());
  checkErrors(conn);

  song_ = unique_song(mpd_run_current_song(conn), &mpd_song_free);
  checkErrors(conn);
}

void waybar::modules::MPD::waitForEvent() {
  auto conn = alternate_connection_.get();
  // Wait for a player (play/pause), option (random, shuffle, etc.), or playlist
  // change
  mpd_run_idle_mask(conn,
                    static_cast<mpd_idle>(MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS | MPD_IDLE_PLAYLIST));
  checkErrors(conn);
}

bool waybar::modules::MPD::handlePlayPause(GdkEventButton* const& e) {
  if (e->type == GDK_2BUTTON_PRESS || e->type == GDK_3BUTTON_PRESS || connection_ == nullptr) {
    return false;
  }

  if (e->button == 1) {
    std::lock_guard guard(connection_lock_);
    if (stopped()) {
      mpd_run_play(connection_.get());
    } else {
      mpd_run_toggle_pause(connection_.get());
    }
  } else if (e->button == 3) {
    std::lock_guard guard(connection_lock_);
    mpd_run_stop(connection_.get());
  }

  return true;
}

bool waybar::modules::MPD::stopped() {
  return connection_ == nullptr || state_ == MPD_STATE_UNKNOWN || state_ == MPD_STATE_STOP;
}

bool waybar::modules::MPD::playing() { return connection_ != nullptr && state_ == MPD_STATE_PLAY; }
