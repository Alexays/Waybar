#include "modules/mpris/mpris.hpp"

#include <fmt/core.h>

#include <optional>
#include <sstream>
#include <string>

#include "util/scope_guard.hpp"

extern "C" {
#include <playerctl/playerctl.h>
}

#include <glib.h>
#include <spdlog/spdlog.h>

namespace waybar::modules::mpris {

const std::string DEFAULT_FORMAT = "{player} ({status}): {dynamic}";

Mpris::Mpris(const std::string& id, const Json::Value& config)
    : ALabel(config, "mpris", id, DEFAULT_FORMAT, 0, false, true),
      tooltip_(DEFAULT_FORMAT),
      artist_len_(-1),
      album_len_(-1),
      title_len_(-1),
      dynamic_len_(-1),
      dynamic_prio_({"title", "artist", "album", "position", "length"}),
      dynamic_order_({"title", "artist", "album", "position", "length"}),
      dynamic_separator_(" - "),
      truncate_hours_(true),
      tooltip_len_limits_(false),
      // this character is used in Gnome so it's fine to use it here
      ellipsis_("\u2026"),
      player_("playerctld"),
      manager(),
      player(),
      last_update_(std::chrono::system_clock::now() - interval_) {
  if (config_["format-playing"].isString()) {
    format_playing_ = config_["format-playing"].asString();
  }
  if (config_["format-paused"].isString()) {
    format_paused_ = config_["format-paused"].asString();
  }
  if (config_["format-stopped"].isString()) {
    format_stopped_ = config_["format-stopped"].asString();
  }
  if (config_["ellipsis"].isString()) {
    ellipsis_ = config_["ellipsis"].asString();
  }
  if (config_["dynamic-separator"].isString()) {
    dynamic_separator_ = config_["dynamic-separator"].asString();
  }
  if (tooltipEnabled()) {
    if (config_["tooltip-format"].isString()) {
      tooltip_ = config_["tooltip-format"].asString();
    }
    if (config_["tooltip-format-playing"].isString()) {
      tooltip_playing_ = config_["tooltip-format-playing"].asString();
    }
    if (config_["tooltip-format-paused"].isString()) {
      tooltip_paused_ = config_["tooltip-format-paused"].asString();
    }
    if (config_["tooltip-format-stopped"].isString()) {
      tooltip_stopped_ = config_["tooltip-format-stopped"].asString();
    }
    if (config_["enable-tooltip-len-limits"].isBool()) {
      tooltip_len_limits_ = config["enable-tooltip-len-limits"].asBool();
    }
  }

  if (config["artist-len"].isUInt()) {
    artist_len_ = config["artist-len"].asUInt();
  }
  if (config["album-len"].isUInt()) {
    album_len_ = config["album-len"].asUInt();
  }
  if (config["title-len"].isUInt()) {
    title_len_ = config["title-len"].asUInt();
  }
  if (config["dynamic-len"].isUInt()) {
    dynamic_len_ = config["dynamic-len"].asUInt();
  }
  // "dynamic-priority" has been kept for backward compatibility
  if (config_["dynamic-importance-order"].isArray() || config_["dynamic-priority"].isArray()) {
    dynamic_prio_.clear();
    const auto& dynamic_priority = config_["dynamic-importance-order"].isArray()
                                       ? config_["dynamic-importance-order"]
                                       : config_["dynamic-priority"];
    for (const auto& value : dynamic_priority) {
      if (value.isString()) {
        dynamic_prio_.push_back(value.asString());
      }
    }
  }
  if (config_["dynamic-order"].isArray()) {
    dynamic_order_.clear();
    for (const auto& item : config_["dynamic-order"]) {
      if (item.isString()) {
        dynamic_order_.push_back(item.asString());
      }
    }
  }

  if (config_["truncate-hours"].isBool()) {
    truncate_hours_ = config["truncate-hours"].asBool();
  }
  if (config_["player"].isString()) {
    player_ = config_["player"].asString();
  }
  if (config_["ignored-players"].isArray()) {
    for (const auto& item : config_["ignored-players"]) {
      if (item.isString()) {
        ignored_players_.push_back(item.asString());
      }
    }
  }

  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([error]() {
    if (error) {
      g_error_free(error);
    }
  });
  manager = playerctl_player_manager_new(&error);
  if (error) {
    throw std::runtime_error(fmt::format("unable to create MPRIS client: {}", error->message));
  }

  g_object_connect(manager, "signal::name-appeared", G_CALLBACK(onPlayerNameAppeared), this, NULL);
  g_object_connect(manager, "signal::name-vanished", G_CALLBACK(onPlayerNameVanished), this, NULL);

  if (player_ == "playerctld") {
    // use playerctld proxy
    PlayerctlPlayerName name = {
        .instance = (gchar*)player_.c_str(),
        .source = PLAYERCTL_SOURCE_DBUS_SESSION,
    };
    player = playerctl_player_new_from_name(&name, &error);

  } else {
    GList* players = playerctl_list_players(&error);
    if (error) {
      throw std::runtime_error(fmt::format("unable to list players: {}", error->message));
    }

    for (auto* p = players; p != nullptr; p = p->next) {
      auto* pn = static_cast<PlayerctlPlayerName*>(p->data);
      if (strcmp(pn->name, player_.c_str()) == 0) {
        player = playerctl_player_new_from_name(pn, &error);
        break;
      }
    }
  }

  if (error) {
    throw std::runtime_error(
        fmt::format("unable to connect to player {}: {}", player_, error->message));
  }

  if (player) {
    g_object_connect(player, "signal::play", G_CALLBACK(onPlayerPlay), this, "signal::pause",
                     G_CALLBACK(onPlayerPause), this, "signal::stop", G_CALLBACK(onPlayerStop),
                     this, "signal::stop", G_CALLBACK(onPlayerStop), this, "signal::metadata",
                     G_CALLBACK(onPlayerMetadata), this, NULL);
  }

  // allow setting an interval count that triggers periodic refreshes
  if (interval_.count() > 0) {
    thread_ = [this] {
      dp.emit();
      thread_.sleep_for(interval_);
    };
  }

  // trigger initial update
  dp.emit();
}

Mpris::~Mpris() {
  if (manager != nullptr) g_object_unref(manager);
  if (player != nullptr) g_object_unref(player);
}

auto Mpris::getIconFromJson(const Json::Value& icons, const std::string& key) -> std::string {
  if (icons.isObject()) {
    if (icons[key].isString()) return icons[key].asString();
    if (icons["default"].isString()) return icons["default"].asString();
  }
  return "";
}

// Wide characters count as two, zero-width characters count as zero
// Modifies str in-place (unless width = std::string::npos)
// Returns the total width of the string pre-truncating
size_t utf8_truncate(std::string& str, size_t width = std::string::npos) {
  if (str.length() == 0) return 0;

  const gchar* trunc_end = nullptr;

  size_t total_width = 0;

  for (gchar *data = str.data(), *end = data + str.size(); data != nullptr;) {
    gunichar c = g_utf8_get_char_validated(data, end - data);
    if (c == -1U || c == -2U) {
      // invalid unicode, treat string as ascii
      if (width != std::string::npos && str.length() > width) str.resize(width);
      return str.length();
    } else if (g_unichar_iswide(c)) {
      total_width += 2;
    } else if (!g_unichar_iszerowidth(c) && c != 0xAD) {  // neither zero-width nor soft hyphen
      total_width += 1;
    }

    data = g_utf8_find_next_char(data, end);
    if (width != std::string::npos && total_width <= width && !g_unichar_isspace(c))
      trunc_end = data;
  }

  if (trunc_end) str.resize(trunc_end - str.data());

  return total_width;
}

size_t utf8_width(const std::string& str) { return utf8_truncate(const_cast<std::string&>(str)); }

void truncate(std::string& s, const std::string& ellipsis, size_t max_len) {
  if (max_len == 0) {
    s.resize(0);
    return;
  }
  size_t len = utf8_truncate(s, max_len);
  if (len > max_len) {
    size_t ellipsis_len = utf8_width(ellipsis);
    if (max_len >= ellipsis_len) {
      if (ellipsis_len) utf8_truncate(s, max_len - ellipsis_len);
      s += ellipsis;
    } else {
      s.resize(0);
    }
  }
}

auto Mpris::getArtistStr(const PlayerInfo& info, bool truncated) -> std::string {
  auto artist = info.artist.value_or(std::string());
  if (truncated && artist_len_ >= 0) truncate(artist, ellipsis_, artist_len_);
  return artist;
}

auto Mpris::getAlbumStr(const PlayerInfo& info, bool truncated) -> std::string {
  auto album = info.album.value_or(std::string());
  if (truncated && album_len_ >= 0) truncate(album, ellipsis_, album_len_);
  return album;
}

auto Mpris::getTitleStr(const PlayerInfo& info, bool truncated) -> std::string {
  auto title = info.title.value_or(std::string());
  if (truncated && title_len_ >= 0) truncate(title, ellipsis_, title_len_);
  return title;
}

auto Mpris::getLengthStr(const PlayerInfo& info, bool truncated) -> std::string {
  if (info.length.has_value()) {
    auto length = info.length.value();
    return (truncated && length.substr(0, 3) == "00:") ? length.substr(3) : length;
  }
  return {};
}

auto Mpris::getPositionStr(const PlayerInfo& info, bool truncated) -> std::string {
  if (info.position.has_value()) {
    auto position = info.position.value();
    return (truncated && position.substr(0, 3) == "00:") ? position.substr(3) : position;
  }
  return {};
}

auto Mpris::getDynamicStr(const PlayerInfo& info, bool truncated, bool html) -> std::string {
  auto artist = getArtistStr(info, truncated);
  auto album = getAlbumStr(info, truncated);
  auto title = getTitleStr(info, truncated);
  auto length = getLengthStr(info, truncated && truncate_hours_);
  // keep position format same as length format
  auto position = getPositionStr(info, truncated && truncate_hours_ && length.length() < 6);

  size_t artistLen = utf8_width(artist);
  size_t albumLen = utf8_width(album);
  size_t titleLen = utf8_width(title);
  size_t lengthLen = length.length();
  size_t posLen = position.length();

  bool showArtist = (artistLen != 0) && (std::find(dynamic_order_.begin(), dynamic_order_.end(),
                                                   "artist") != dynamic_order_.end());
  bool showAlbum = (albumLen != 0) && (std::find(dynamic_order_.begin(), dynamic_order_.end(),
                                                 "album") != dynamic_order_.end());
  bool showTitle = (titleLen != 0) && (std::find(dynamic_order_.begin(), dynamic_order_.end(),
                                                 "title") != dynamic_order_.end());
  bool showLength = (lengthLen != 0) && (std::find(dynamic_order_.begin(), dynamic_order_.end(),
                                                   "length") != dynamic_order_.end());
  bool showPos = (posLen != 0) && (std::find(dynamic_order_.begin(), dynamic_order_.end(),
                                             "position") != dynamic_order_.end());

  if (truncated && dynamic_len_ >= 0) {
    // Since the first element doesn't present a separator and we don't know a priori which one
    // it will be, we add a "virtual separatorLen" to the dynamicLen, since we are adding the
    // separatorLen to all the other lengths.
    size_t separatorLen = utf8_width(dynamic_separator_);
    size_t dynamicLen = dynamic_len_ + separatorLen;
    if (showArtist) artistLen += separatorLen;
    if (showAlbum) albumLen += separatorLen;
    if (showTitle) albumLen += separatorLen;
    if (showLength) lengthLen += separatorLen;
    if (showPos) posLen += separatorLen;

    size_t totalLen = 0;

    for (const auto& item : dynamic_prio_) {
      if (item == "artist") {
        if (totalLen + artistLen > dynamicLen) {
          showArtist = false;
        } else if (showArtist) {
          totalLen += artistLen;
        }
      } else if (item == "album") {
        if (totalLen + albumLen > dynamicLen) {
          showAlbum = false;
        } else if (showAlbum) {
          totalLen += albumLen;
        }
      } else if (item == "title") {
        if (totalLen + titleLen > dynamicLen) {
          showTitle = false;
        } else if (showTitle) {
          totalLen += titleLen;
        }
      } else if (item == "length") {
        if (totalLen + lengthLen > dynamicLen) {
          showLength = false;
        } else if (showLength) {
          totalLen += lengthLen;
          posLen = std::max((size_t)2, posLen) - 2;
        }
      } else if (item == "position") {
        if (totalLen + posLen > dynamicLen) {
          showPos = false;
        } else if (showPos) {
          totalLen += posLen;
          lengthLen = std::max((size_t)2, lengthLen) - 2;
        }
      }
    }
  }

  std::stringstream dynamic;
  if (html) {
    artist = Glib::Markup::escape_text(artist);
    album = Glib::Markup::escape_text(album);
    title = Glib::Markup::escape_text(title);
  }

  bool lengthOrPositionShown = false;
  bool previousShown = false;
  std::string previousOrder = "";

  for (const std::string& order : dynamic_order_) {
    if ((order == "artist" && showArtist) || (order == "album" && showAlbum) ||
        (order == "title" && showTitle)) {
      if (previousShown && previousOrder != "length" && previousOrder != "position") {
        dynamic << dynamic_separator_;
      }

      if (order == "artist") {
        dynamic << artist;
      } else if (order == "album") {
        dynamic << album;
      } else if (order == "title") {
        dynamic << title;
      }

      previousShown = true;
    } else if (order == "length" || order == "position") {
      if (!lengthOrPositionShown && (showLength || showPos)) {
        if (html) dynamic << "<small>";
        if (previousShown) dynamic << ' ';
        dynamic << '[';
        if (showPos) {
          dynamic << position;
          if (showLength) dynamic << '/';
        }
        if (showLength) dynamic << length;
        dynamic << ']';
        if (!dynamic.str().empty()) dynamic << ' ';
        if (html) dynamic << "</small>";
        lengthOrPositionShown = true;
      }
    }
    previousOrder = order;
  }
  return dynamic.str();
}

auto Mpris::onPlayerNameAppeared(PlayerctlPlayerManager* manager, PlayerctlPlayerName* player_name,
                                 gpointer data) -> void {
  auto* mpris = static_cast<Mpris*>(data);
  if (!mpris) return;

  spdlog::debug("mpris: name-appeared callback: {}", player_name->name);

  if (std::string(player_name->name) != mpris->player_) {
    return;
  }

  mpris->player = playerctl_player_new_from_name(player_name, nullptr);
  g_object_connect(mpris->player, "signal::play", G_CALLBACK(onPlayerPlay), mpris, "signal::pause",
                   G_CALLBACK(onPlayerPause), mpris, "signal::stop", G_CALLBACK(onPlayerStop),
                   mpris, "signal::stop", G_CALLBACK(onPlayerStop), mpris, "signal::metadata",
                   G_CALLBACK(onPlayerMetadata), mpris, NULL);

  mpris->dp.emit();
}

auto Mpris::onPlayerNameVanished(PlayerctlPlayerManager* manager, PlayerctlPlayerName* player_name,
                                 gpointer data) -> void {
  auto* mpris = static_cast<Mpris*>(data);
  if (!mpris) return;

  spdlog::debug("mpris: player-vanished callback: {}", player_name->name);

  if (std::string(player_name->name) == mpris->player_) {
    mpris->player = nullptr;
    mpris->event_box_.set_visible(false);
    mpris->dp.emit();
  }
}

auto Mpris::onPlayerPlay(PlayerctlPlayer* player, gpointer data) -> void {
  auto* mpris = static_cast<Mpris*>(data);
  if (!mpris) return;

  spdlog::debug("mpris: player-play callback");
  // update widget
  mpris->dp.emit();
}

auto Mpris::onPlayerPause(PlayerctlPlayer* player, gpointer data) -> void {
  auto* mpris = static_cast<Mpris*>(data);
  if (!mpris) return;

  spdlog::debug("mpris: player-pause callback");
  // update widget
  mpris->dp.emit();
}

auto Mpris::onPlayerStop(PlayerctlPlayer* player, gpointer data) -> void {
  auto* mpris = static_cast<Mpris*>(data);
  if (!mpris) return;

  spdlog::debug("mpris: player-stop callback");

  // hide widget
  mpris->event_box_.set_visible(false);
  // update widget
  mpris->dp.emit();
}

auto Mpris::onPlayerMetadata(PlayerctlPlayer* player, GVariant* metadata, gpointer data) -> void {
  auto* mpris = static_cast<Mpris*>(data);
  if (!mpris) return;

  spdlog::debug("mpris: player-metadata callback");
  // update widget
  mpris->dp.emit();
}

auto Mpris::getPlayerInfo() -> std::optional<PlayerInfo> {
  if (!player) {
    return std::nullopt;
  }

  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([error]() {
    if (error) {
      g_error_free(error);
    }
  });

  char* player_status = nullptr;
  auto player_playback_status = PLAYERCTL_PLAYBACK_STATUS_STOPPED;
  g_object_get(player, "status", &player_status, "playback-status", &player_playback_status, NULL);

  std::string player_name = player_;
  if (player_name == "playerctld") {
    GList* players = playerctl_list_players(&error);
    if (error) {
      throw std::runtime_error(fmt::format("unable to list players: {}", error->message));
    }
    // > get the list of players [..] in order of activity
    // https://github.com/altdesktop/playerctl/blob/b19a71cb9dba635df68d271bd2b3f6a99336a223/playerctl/playerctl-common.c#L248-L249
    players = g_list_first(players);
    if (players) player_name = static_cast<PlayerctlPlayerName*>(players->data)->name;
  }

  if (std::any_of(ignored_players_.begin(), ignored_players_.end(),
                  [&](const std::string& pn) { return player_name == pn; })) {
    spdlog::warn("mpris[{}]: ignoring player update", player_name);
    return std::nullopt;
  }

  // make status lowercase
  player_status[0] = std::tolower(player_status[0]);

  PlayerInfo info = {
      .name = player_name,
      .status = player_playback_status,
      .status_string = player_status,
      .artist = std::nullopt,
      .album = std::nullopt,
      .title = std::nullopt,
      .length = std::nullopt,
  };

  if (auto* artist_ = playerctl_player_get_artist(player, &error)) {
    spdlog::debug("mpris[{}]: artist = {}", info.name, artist_);
    info.artist = artist_;
    g_free(artist_);
  }
  if (error) goto errorexit;

  if (auto* album_ = playerctl_player_get_album(player, &error)) {
    spdlog::debug("mpris[{}]: album = {}", info.name, album_);
    info.album = album_;
    g_free(album_);
  }
  if (error) goto errorexit;

  if (auto* title_ = playerctl_player_get_title(player, &error)) {
    spdlog::debug("mpris[{}]: title = {}", info.name, title_);
    info.title = title_;
    g_free(title_);
  }
  if (error) goto errorexit;

  if (auto* length_ = playerctl_player_print_metadata_prop(player, "mpris:length", &error)) {
    spdlog::debug("mpris[{}]: mpris:length = {}", info.name, length_);
    auto len = std::chrono::microseconds(std::strtol(length_, nullptr, 10));
    auto len_h = std::chrono::duration_cast<std::chrono::hours>(len);
    auto len_m = std::chrono::duration_cast<std::chrono::minutes>(len - len_h);
    auto len_s = std::chrono::duration_cast<std::chrono::seconds>(len - len_h - len_m);
    info.length = fmt::format("{:02}:{:02}:{:02}", len_h.count(), len_m.count(), len_s.count());
    g_free(length_);
  }
  if (error) goto errorexit;

  {
    auto position_ = playerctl_player_get_position(player, &error);
    if (error) {
      // it's fine to have an error here because not all players report a position
      g_error_free(error);
      error = nullptr;
    } else {
      spdlog::debug("mpris[{}]: position = {}", info.name, position_);
      auto len = std::chrono::microseconds(position_);
      auto len_h = std::chrono::duration_cast<std::chrono::hours>(len);
      auto len_m = std::chrono::duration_cast<std::chrono::minutes>(len - len_h);
      auto len_s = std::chrono::duration_cast<std::chrono::seconds>(len - len_h - len_m);
      info.position = fmt::format("{:02}:{:02}:{:02}", len_h.count(), len_m.count(), len_s.count());
    }
  }

  return info;

errorexit:
  std::string errorMsg = error->message;
  //  When mpris checks for  active player sessions periodically(5 secs), NoActivePlayer error
  //  message is
  // thrown when there are no active sessions. This error message is spamming logs without having
  // any value addition. Log the error only if the error we recceived is not NoActivePlayer.
  if (errorMsg.rfind("GDBus.Error:com.github.altdesktop.playerctld.NoActivePlayer") ==
      std::string::npos) {
    spdlog::error("mpris[{}]: {}", info.name, error->message);
  }
  return std::nullopt;
}

bool Mpris::handleToggle(GdkEventButton* const& e) {
  GError* error = nullptr;
  waybar::util::ScopeGuard error_deleter([error]() {
    if (error) {
      g_error_free(error);
    }
  });

  auto info = getPlayerInfo();
  if (!info) return false;

  if (e->type == GdkEventType::GDK_BUTTON_PRESS) {
    switch (e->button) {
      case 1:  // left-click
        if (config_["on-click"].isString()) {
          return ALabel::handleToggle(e);
        }
        playerctl_player_play_pause(player, &error);
        break;
      case 2:  // middle-click
        if (config_["on-click-middle"].isString()) {
          return ALabel::handleToggle(e);
        }
        playerctl_player_previous(player, &error);
        break;
      case 3:  // right-click
        if (config_["on-click-right"].isString()) {
          return ALabel::handleToggle(e);
        }
        playerctl_player_next(player, &error);
        break;
    }
  }
  if (error) {
    spdlog::error("mpris[{}]: error running builtin on-click action: {}", (*info).name,
                  error->message);
    return false;
  }
  return true;
}

auto Mpris::update() -> void {
  const auto now = std::chrono::system_clock::now();
  if (now - last_update_ < interval_) return;
  last_update_ = now;

  auto opt = getPlayerInfo();
  if (!opt) {
    event_box_.set_visible(false);
    ALabel::update();
    return;
  }
  auto info = *opt;

  if (info.status == PLAYERCTL_PLAYBACK_STATUS_STOPPED) {
    spdlog::debug("mpris[{}]: player stopped, skipping update", info.name);
    return;
  }

  spdlog::debug("mpris[{}]: running update", info.name);

  // set css class for player status
  if (!lastStatus.empty() && label_.get_style_context()->has_class(lastStatus)) {
    label_.get_style_context()->remove_class(lastStatus);
  }
  if (!label_.get_style_context()->has_class(info.status_string)) {
    label_.get_style_context()->add_class(info.status_string);
  }
  lastStatus = info.status_string;

  // set css class for player name
  if (!lastPlayer.empty() && label_.get_style_context()->has_class(lastPlayer)) {
    label_.get_style_context()->remove_class(lastPlayer);
  }
  if (!label_.get_style_context()->has_class(info.name)) {
    label_.get_style_context()->add_class(info.name);
  }
  lastPlayer = info.name;

  auto formatstr = format_;
  auto tooltipstr = tooltip_;
  switch (info.status) {
    case PLAYERCTL_PLAYBACK_STATUS_PLAYING:
      if (!format_playing_.empty()) formatstr = format_playing_;
      if (!tooltip_playing_.empty()) tooltipstr = tooltip_playing_;
      break;
    case PLAYERCTL_PLAYBACK_STATUS_PAUSED:
      if (!format_paused_.empty()) formatstr = format_paused_;
      if (!tooltip_paused_.empty()) tooltipstr = tooltip_paused_;
      break;
    case PLAYERCTL_PLAYBACK_STATUS_STOPPED:
      if (!format_stopped_.empty()) formatstr = format_stopped_;
      if (!tooltip_stopped_.empty()) tooltipstr = tooltip_stopped_;
      break;
  }

  std::string length = getLengthStr(info, truncate_hours_);
  std::string tooltipLength =
      (tooltip_len_limits_ || length.length() > 5) ? length : getLengthStr(info, false);
  // keep position format same as length format
  std::string position = getPositionStr(info, truncate_hours_ && length.length() < 6);
  std::string tooltipPosition =
      (tooltip_len_limits_ || position.length() > 5) ? position : getPositionStr(info, false);

  try {
    auto label_format = fmt::format(
        fmt::runtime(formatstr),
        fmt::arg("player", std::string(Glib::Markup::escape_text(info.name))),
        fmt::arg("status", info.status_string),
        fmt::arg("artist", std::string(Glib::Markup::escape_text(getArtistStr(info, true)))),
        fmt::arg("title", std::string(Glib::Markup::escape_text(getTitleStr(info, true)))),
        fmt::arg("album", std::string(Glib::Markup::escape_text(getAlbumStr(info, true)))),
        fmt::arg("length", length), fmt::arg("position", position),
        fmt::arg("dynamic", getDynamicStr(info, true, true)),
        fmt::arg("player_icon", getIconFromJson(config_["player-icons"], info.name)),
        fmt::arg("status_icon", getIconFromJson(config_["status-icons"], info.status_string)));

    if (label_format.empty()) {
      label_.hide();
    } else {
      label_.set_markup(label_format);
      label_.show();
    }
  } catch (fmt::format_error const& e) {
    spdlog::warn("mpris: format error: {}", e.what());
  }

  if (tooltipEnabled()) {
    try {
      auto tooltip_text = fmt::format(
          fmt::runtime(tooltipstr), fmt::arg("player", info.name),
          fmt::arg("status", info.status_string),
          fmt::arg("artist", getArtistStr(info, tooltip_len_limits_)),
          fmt::arg("title", getTitleStr(info, tooltip_len_limits_)),
          fmt::arg("album", getAlbumStr(info, tooltip_len_limits_)),
          fmt::arg("length", tooltipLength), fmt::arg("position", tooltipPosition),
          fmt::arg("dynamic", getDynamicStr(info, tooltip_len_limits_, false)),
          fmt::arg("player_icon", getIconFromJson(config_["player-icons"], info.name)),
          fmt::arg("status_icon", getIconFromJson(config_["status-icons"], info.status_string)));

      label_.set_tooltip_text(tooltip_text);
    } catch (fmt::format_error const& e) {
      spdlog::warn("mpris: format error (tooltip): {}", e.what());
    }
  }

  event_box_.set_visible(true);
  // call parent update
  ALabel::update();
}

}  // namespace waybar::modules::mpris
