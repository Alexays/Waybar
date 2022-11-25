#include "modules/mpd/mpd.hpp"

#include <fmt/chrono.h>
#include <glibmm/ustring.h>
#include <spdlog/spdlog.h>

#include <util/sanitize_str.hpp>
using namespace waybar::util;

#include "modules/mpd/state.hpp"
#if defined(MPD_NOINLINE)
namespace waybar::modules {
#include "modules/mpd/state.inl.hpp"
}  // namespace waybar::modules
#endif

waybar::modules::MPD::MPD(const std::string& id, const Json::Value& config)
    : ALabel(config, "mpd", id, "{album} - {artist} - {title}", 5, false, true),
      module_name_(id.empty() ? "mpd" : "mpd#" + id),
      server_(nullptr),
      port_(config_["port"].isUInt() ? config["port"].asUInt() : 0),
      password_(config_["password"].empty() ? "" : config_["password"].asString()),
      timeout_(config_["timeout"].isUInt() ? config_["timeout"].asUInt() * 1'000 : 30'000),
      connection_(nullptr, &mpd_connection_free),
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

  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &MPD::handlePlayPause));
}

auto waybar::modules::MPD::update() -> void {
  context_.update();

  // Call parent update
  ALabel::update();
}

void waybar::modules::MPD::queryMPD() {
  if (connection_ != nullptr) {
    spdlog::debug("{}: fetching state information", module_name_);
    try {
      fetchState();
      spdlog::debug("{}: fetch complete", module_name_);
    } catch (std::exception const& e) {
      spdlog::error("{}: {}", module_name_, e.what());
      state_ = MPD_STATE_UNKNOWN;
    }

    dp.emit();
  }
}

std::string waybar::modules::MPD::getTag(mpd_tag_type type, unsigned idx) const {
  std::string result =
      config_["unknown-tag"].isString() ? config_["unknown-tag"].asString() : "N/A";
  const char* tag = mpd_song_get_tag(song_.get(), type, idx);

  // mpd_song_get_tag can return NULL, so make sure it's valid before setting
  if (tag) result = tag;

  return result;
}

std::string waybar::modules::MPD::getFilename() const {
  std::string path = mpd_song_get_uri(song_.get());
  size_t position = path.find_last_of("/");
  if (position == std::string::npos) {
    return path;
  } else {
    return path.substr(position + 1);
  }
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
  Glib::ustring artist, album_artist, album, title;
  std::string date, filename;
  int song_pos = 0, queue_length = 0, volume = 0;
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
      format = config_["format-paused"].isString() ? config_["format-paused"].asString()
                                                   : config_["format"].asString();
      label_.get_style_context()->add_class("paused");
      label_.get_style_context()->remove_class("playing");
    }

    stateIcon = getStateIcon();

    artist = sanitize_string(getTag(MPD_TAG_ARTIST));
    album_artist = sanitize_string(getTag(MPD_TAG_ALBUM_ARTIST));
    album = sanitize_string(getTag(MPD_TAG_ALBUM));
    title = sanitize_string(getTag(MPD_TAG_TITLE));
    date = sanitize_string(getTag(MPD_TAG_DATE));
    filename = sanitize_string(getFilename());
    song_pos = mpd_status_get_song_pos(status_.get()) + 1;
    volume = mpd_status_get_volume(status_.get());
    if (volume < 0) {
      volume = 0;
    }
    queue_length = mpd_status_get_queue_length(status_.get());
    elapsedTime = std::chrono::seconds(mpd_status_get_elapsed_time(status_.get()));
    totalTime = std::chrono::seconds(mpd_status_get_total_time(status_.get()));
  }

  bool consumeActivated = mpd_status_get_consume(status_.get());
  std::string consumeIcon = getOptionIcon("consume", consumeActivated);
  bool randomActivated = mpd_status_get_random(status_.get());
  std::string randomIcon = getOptionIcon("random", randomActivated);
  bool repeatActivated = mpd_status_get_repeat(status_.get());
  std::string repeatIcon = getOptionIcon("repeat", repeatActivated);
  bool singleActivated = mpd_status_get_single(status_.get());
  std::string singleIcon = getOptionIcon("single", singleActivated);
  if (config_["artist-len"].isInt()) artist = artist.substr(0, config_["artist-len"].asInt());
  if (config_["album-artist-len"].isInt())
    album_artist = album_artist.substr(0, config_["album-artist-len"].asInt());
  if (config_["album-len"].isInt()) album = album.substr(0, config_["album-len"].asInt());
  if (config_["title-len"].isInt()) title = title.substr(0, config_["title-len"].asInt());

  try {
    label_.set_markup(fmt::format(
        format, fmt::arg("artist", artist.raw()), fmt::arg("albumArtist", album_artist.raw()),
        fmt::arg("album", album.raw()), fmt::arg("title", title.raw()), fmt::arg("date", date),
        fmt::arg("volume", volume), fmt::arg("elapsedTime", elapsedTime),
        fmt::arg("totalTime", totalTime), fmt::arg("songPosition", song_pos),
        fmt::arg("queueLength", queue_length), fmt::arg("stateIcon", stateIcon),
        fmt::arg("consumeIcon", consumeIcon), fmt::arg("randomIcon", randomIcon),
        fmt::arg("repeatIcon", repeatIcon), fmt::arg("singleIcon", singleIcon),
        fmt::arg("filename", filename)));
  } catch (fmt::format_error const& e) {
    spdlog::warn("mpd: format error: {}", e.what());
  }

  if (tooltipEnabled()) {
    std::string tooltip_format;
    tooltip_format = config_["tooltip-format"].isString() ? config_["tooltip-format"].asString()
                                                          : "MPD (connected)";
    try {
      auto tooltip_text =
          fmt::format(tooltip_format, fmt::arg("artist", artist.raw()),
                      fmt::arg("albumArtist", album_artist.raw()), fmt::arg("album", album.raw()),
                      fmt::arg("title", title.raw()), fmt::arg("date", date),
                      fmt::arg("volume", volume), fmt::arg("elapsedTime", elapsedTime),
                      fmt::arg("totalTime", totalTime), fmt::arg("songPosition", song_pos),
                      fmt::arg("queueLength", queue_length), fmt::arg("stateIcon", stateIcon),
                      fmt::arg("consumeIcon", consumeIcon), fmt::arg("randomIcon", randomIcon),
                      fmt::arg("repeatIcon", repeatIcon), fmt::arg("singleIcon", singleIcon));
      label_.set_tooltip_text(tooltip_text);
    } catch (fmt::format_error const& e) {
      spdlog::warn("mpd: format error (tooltip): {}", e.what());
    }
  }
}

std::string waybar::modules::MPD::getStateIcon() const {
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

std::string waybar::modules::MPD::getOptionIcon(std::string optionName, bool activated) const {
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
      detail::unique_connection(mpd_connection_new(server_, port_, timeout_), &mpd_connection_free);

  if (connection_ == nullptr) {
    spdlog::error("{}: Failed to connect to MPD", module_name_);
    connection_.reset();
    return;
  }

  try {
    checkErrors(connection_.get());
    spdlog::debug("{}: Connected to MPD", module_name_);

    if (!password_.empty()) {
      bool res = mpd_run_password(connection_.get(), password_.c_str());
      if (!res) {
        spdlog::error("{}: Wrong MPD password", module_name_);
        connection_.reset();
        return;
      }
      checkErrors(connection_.get());
    }
  } catch (std::runtime_error& e) {
    spdlog::error("{}: Failed to connect to MPD: {}", module_name_, e.what());
    connection_.reset();
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
      state_ = MPD_STATE_UNKNOWN;
      throw std::runtime_error("Connection to MPD closed");
    default:
      if (conn) {
        auto error_message = mpd_connection_get_error_message(conn);
        std::string error(error_message);
        mpd_connection_clear_error(conn);
        throw std::runtime_error(error);
      }
      throw std::runtime_error("Invalid connection");
  }
}

void waybar::modules::MPD::fetchState() {
  if (connection_ == nullptr) {
    spdlog::error("{}: Not connected to MPD", module_name_);
    return;
  }

  auto conn = connection_.get();

  status_ = detail::unique_status(mpd_run_status(conn), &mpd_status_free);
  checkErrors(conn);

  state_ = mpd_status_get_state(status_.get());
  checkErrors(conn);

  song_ = detail::unique_song(mpd_run_current_song(conn), &mpd_song_free);
  checkErrors(conn);
}

bool waybar::modules::MPD::handlePlayPause(GdkEventButton* const& e) {
  if (e->type == GDK_2BUTTON_PRESS || e->type == GDK_3BUTTON_PRESS || connection_ == nullptr) {
    return false;
  }

  if (e->button == 1) {
    if (state_ == MPD_STATE_PLAY)
      context_.pause();
    else
      context_.play();
  } else if (e->button == 3) {
    context_.stop();
  }

  return true;
}
