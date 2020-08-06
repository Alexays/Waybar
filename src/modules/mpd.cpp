#include "modules/mpd.hpp"

#include <fmt/chrono.h>
#include <spdlog/spdlog.h>

waybar::modules::MPD::MPD(const std::string& id, const Json::Value& config)
    : ALabel(config, "mpd", id, "{album} - {artist} - {title}", 5),
      module_name_(id.empty() ? "mpd" : "mpd#" + id),
      server_(nullptr),
      port_(config_["port"].isUInt() ? config["port"].asUInt() : 0),
      timeout_(config_["timeout"].isUInt() ? config_["timeout"].asUInt() * 1'000 : 30'000),
      connection_(nullptr, &mpd_connection_free),
      alternate_connection_(nullptr, &mpd_connection_free),
      status_(nullptr, &mpd_status_free),
      song_(nullptr, &mpd_song_free) {
  if (!config_["port"].isNull() && !config_["port"].isUInt()) {
    spdlog::warn("{}: `port` configuration should be an unsigned int", module_name_);
  }

  if (!config_["timeout"].isNull() && !config_["timeout"].isUInt()) {
    spdlog::warn("{}: `timeout` configuration should be an unsigned int", module_name_);
  }

  if (!config["server"].isNull()) {
    if (!config_["server"].isString()) {
      spdlog::warn("{}:`server` configuration should be a string", module_name_);
    }
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
      if(!wasPlaying) {
        // Wait until the periodic_updater has stopped
        std::lock_guard periodic_guard(periodic_lock_);
      }
      fetchState();
      if (!wasPlaying && playing()) {
        periodic_updater().detach();
      }
    } catch (const std::exception& e) {
      spdlog::error("{}: {}", module_name_, e.what());
      state_ = MPD_STATE_UNKNOWN;
    }
  }

  setLabel();

  // Call parent update
  ALabel::update();
}

std::thread waybar::modules::MPD::event_listener() {
  return std::thread([this] {
    while (true) {
      try {
        if (connection_ == nullptr) {
          // Retry periodically if no connection
          dp.emit();
          std::this_thread::sleep_for(interval_);
        } else {
          waitForEvent();
          dp.emit();
        }
      } catch (const std::exception& e) {
        if (strcmp(e.what(), "Connection to MPD closed") == 0) {
          spdlog::debug("{}: {}", module_name_, e.what());
        } else {
          spdlog::warn("{}: {}", module_name_, e.what());
        }
      }
    }
  });
}

std::thread waybar::modules::MPD::periodic_updater() {
  return std::thread([this] {
    std::lock_guard guard(periodic_lock_);
    while (connection_ != nullptr && playing()) {
      dp.emit();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });
}

std::string waybar::modules::MPD::getTag(mpd_tag_type type, unsigned idx) {
  std::string result =
      config_["unknown-tag"].isString() ? config_["unknown-tag"].asString() : "N/A";
  const char* tag = mpd_song_get_tag(song_.get(), type, idx);

  // mpd_song_get_tag can return NULL, so make sure it's valid before setting
  if (tag) result = tag;

  return result;
}

void waybar::modules::MPD::setLabel() {
  if (connection_ == nullptr) {
    label_.get_style_context()->add_class("disconnected");
    label_.get_style_context()->remove_class("stopped");
    label_.get_style_context()->remove_class("playing");
    label_.get_style_context()->remove_class("paused");

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
    } else if (paused()) {
      format =
        config_["format-paused"].isString() ? config_["format-paused"].asString() : config_["format"].asString();
      label_.get_style_context()->add_class("paused");
      label_.get_style_context()->remove_class("playing");
    }

    stateIcon = getStateIcon();

    artist = getTag(MPD_TAG_ARTIST);
    album_artist = getTag(MPD_TAG_ALBUM_ARTIST);
    album = getTag(MPD_TAG_ALBUM);
    title = getTag(MPD_TAG_TITLE);
    date = getTag(MPD_TAG_DATE);
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
  label_.set_markup(
      fmt::format(format,
                  fmt::arg("artist", Glib::Markup::escape_text(artist).raw()),
                  fmt::arg("albumArtist", Glib::Markup::escape_text(album_artist).raw()),
                  fmt::arg("album", Glib::Markup::escape_text(album).raw()),
                  fmt::arg("title", Glib::Markup::escape_text(title).raw()),
                  fmt::arg("date", Glib::Markup::escape_text(date).raw()),
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
    spdlog::warn("{}: Trying to fetch state icon while disconnected", module_name_);
    return "";
  }

  if (stopped()) {
    spdlog::warn("{}: Trying to fetch state icon while stopped", module_name_);
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
    spdlog::warn("{}: Trying to fetch option icon while disconnected", module_name_);
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

  connection_ =
      unique_connection(mpd_connection_new(server_, port_, timeout_), &mpd_connection_free);

  alternate_connection_ =
      unique_connection(mpd_connection_new(server_, port_, timeout_), &mpd_connection_free);

  if (connection_ == nullptr || alternate_connection_ == nullptr) {
    spdlog::error("{}: Failed to connect to MPD", module_name_);
    connection_.reset();
    alternate_connection_.reset();
    return;
  }

  try {
    checkErrors(connection_.get());
    spdlog::debug("{}: Connected to MPD", module_name_);
  } catch (std::runtime_error& e) {
    spdlog::error("{}: Failed to connect to MPD: {}", module_name_, e.what());
    connection_.reset();
    alternate_connection_.reset();
  }
}

void waybar::modules::MPD::checkErrors(mpd_connection* conn) {
  switch (mpd_connection_get_error(conn)) {
    case MPD_ERROR_SUCCESS:
      mpd_connection_clear_error(conn);
      return;
    case MPD_ERROR_TIMEOUT:
    case MPD_ERROR_CLOSED:
      mpd_connection_clear_error(conn);
      connection_.reset();
      alternate_connection_.reset();
      state_ = MPD_STATE_UNKNOWN;
      throw std::runtime_error("Connection to MPD closed");
    default:
      if (conn) {
        auto error_message = mpd_connection_get_error_message(conn);
        mpd_connection_clear_error(conn);
        throw std::runtime_error(std::string(error_message));
      }
      throw std::runtime_error("Invalid connection");
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
  if (!mpd_send_idle_mask(
          conn, static_cast<mpd_idle>(MPD_IDLE_PLAYER | MPD_IDLE_OPTIONS | MPD_IDLE_QUEUE))) {
    checkErrors(conn);
    return;
  }
  // alternate_idle_ = true;

  // See issue #277:
  // https://github.com/Alexays/Waybar/issues/277
  mpd_recv_idle(conn, /* disable_timeout = */ false);
  // See issue #281:
  // https://github.com/Alexays/Waybar/issues/281
  std::lock_guard guard(connection_lock_);

  checkErrors(conn);
  mpd_response_finish(conn);

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
  return connection_ == nullptr || state_ == MPD_STATE_UNKNOWN || state_ == MPD_STATE_STOP || status_ == nullptr;
}

bool waybar::modules::MPD::playing() { return connection_ != nullptr && state_ == MPD_STATE_PLAY; }

bool waybar::modules::MPD::paused() { return connection_ != nullptr && state_ == MPD_STATE_PAUSE; }
