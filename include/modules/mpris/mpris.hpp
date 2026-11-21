#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "gtkmm/box.h"
#include "gtkmm/label.h"

extern "C" {
#include <playerctl/playerctl.h>
}

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules::mpris {

class Mpris : public ALabel {
 public:
  Mpris(const std::string&, const Json::Value&);
  virtual ~Mpris();
  auto update() -> void override;
  bool handleToggle(GdkEventButton* const&) override;

 private:
  struct PlayerInfo {
    std::string name;
    PlayerctlPlaybackStatus status;
    std::string status_string;

    std::optional<std::string> artist;
    std::optional<std::string> album;
    std::optional<std::string> album_artist;
    std::optional<std::string> title;
    std::optional<std::string> length;    // as HH:MM:SS
    std::optional<std::string> position;  // same format
  };

  // libplayerctl only makes blocking D-Bus calls, so every playerctl object lives on a thread and
  // GMainContext of its own. The thread is detached and co-owns the worker: nothing waits on it.
  class Worker : public std::enable_shared_from_this<Worker> {
   public:
    using Action = void (*)(PlayerctlPlayer*, GError**);

    Worker(util::CoalescingDispatcher&, std::string, std::vector<std::string>,
           std::chrono::milliseconds);
    ~Worker();
    void start();
    void stop();
    void refresh();
    void perform(Action);
    std::optional<PlayerInfo> getInfo();

   private:
    static auto onPlayerNameAppeared(PlayerctlPlayerManager*, PlayerctlPlayerName*, gpointer)
        -> void;
    static auto onPlayerNameVanished(PlayerctlPlayerManager*, PlayerctlPlayerName*, gpointer)
        -> void;
    static auto onPlayerPlay(PlayerctlPlayer*, gpointer) -> void;
    static auto onPlayerPause(PlayerctlPlayer*, gpointer) -> void;
    static auto onPlayerStop(PlayerctlPlayer*, gpointer) -> void;
    static auto onPlayerMetadata(PlayerctlPlayer*, GVariant*, gpointer) -> void;
    static gboolean releaseVanishedPlayer(gpointer);

    void run();
    void connect();
    void disconnect();
    bool handleRequests();
    auto getPlayerInfo() -> std::optional<PlayerInfo>;
    void dropFallbackPlayer();

    const std::string player_;
    const std::vector<std::string> ignored_players_;
    const std::chrono::milliseconds interval_;
    GMainContext* const context_;

    // Only touched by the thread.
    PlayerctlPlayerManager* manager = nullptr;
    PlayerctlPlayer* player = nullptr;
    // Only set while handleRequests() serves a request (see dropFallbackPlayer()).
    PlayerctlPlayer* last_active_player_ = nullptr;
    GSource* vanished_release_ = nullptr;

    // Never held while emitting: the dispatcher pipe can block until the main thread drains it.
    std::mutex mutex_;
    bool refresh_ = true;
    std::vector<Action> actions_;
    std::optional<PlayerInfo> info_;
    std::chrono::time_point<std::chrono::system_clock> last_update_;

    std::mutex dp_mutex_;
    util::CoalescingDispatcher* dp_;  // null once the module is gone
  };

  auto getIconFromJson(const Json::Value&, const std::string&) -> std::string;
  auto getArtistStr(const PlayerInfo&, bool) -> std::string;
  auto getAlbumStr(const PlayerInfo&, bool) -> std::string;
  auto getTitleStr(const PlayerInfo&, bool) -> std::string;
  auto getLengthStr(const PlayerInfo&, bool) -> std::string;
  auto getPositionStr(const PlayerInfo&, bool) -> std::string;
  auto getDynamicStr(const PlayerInfo&, bool, bool) -> std::string;

  // config
  std::string format_playing_;
  std::string format_paused_;
  std::string format_stopped_;

  std::string tooltip_;
  std::string tooltip_playing_;
  std::string tooltip_paused_;
  std::string tooltip_stopped_;

  int artist_len_;
  int album_len_;
  int title_len_;
  int dynamic_len_;
  std::vector<std::string> dynamic_prio_;
  std::vector<std::string> dynamic_order_;
  std::string dynamic_separator_;
  bool truncate_hours_;
  bool tooltip_len_limits_;
  std::string ellipsis_;

  std::string player_;
  std::vector<std::string> ignored_players_;

  bool prefer_album_artist_;
  bool hide_on_paused_;

  std::string lastStatus;
  std::string lastPlayer;

  std::shared_ptr<Worker> worker_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules::mpris
