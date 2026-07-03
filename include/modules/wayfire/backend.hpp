#pragma once

#include <json/json.h>
#include <unistd.h>

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "util/scoped_fd.hpp"

namespace waybar::modules::wayfire {

using EventHandler = std::function<void(const std::string& event)>;

struct State {
  /*
    ┌───────────┐ ┌───────────┐
    │ output #1 │ │ output #2 │
    └─────┬─────┘ └─────┬─────┘
          └─┐           └─────┐─ ─ ─ ─ ─ ─ ─ ─ ┐
    ┌───────┴───────┐ ┌───────┴──────┐ ┌───────┴───────┐
    │ wset #1       │ │ wset #2      │ │ wset #3       │
    │┌────────────┐ │ │┌────────────┐│ │┌────────────┐ │
    ││ workspaces │ │ ││ workspaces ││ ││ workspaces │ │
    │└─┬──────────┘ │ │└────────────┘│ │└─┬──────────┘ │
    │  │ ┌─────────┐│ └──────────────┘ │  │ ┌─────────┐│
    │  ├─┤ view #1 ││                  │  └─┤ view #3 ││
    │  │ └─────────┘│                  │    └─────────┘│
    │  │ ┌─────────┐│                  └───────────────┘
    │  └─┤ view #2 ││
    │    └─────────┘│
    └───────────────┘
  */

  struct Output {
    size_t id;
    size_t w, h;
    size_t wset_idx;
  };

  struct Workspace {
    size_t num_views;
    size_t num_sticky_views;
  };

  struct Wset {
    std::optional<std::reference_wrapper<Output>> output;
    std::vector<Workspace> wss;
    size_t ws_w, ws_h, ws_x, ws_y;
    size_t focused_view_id;

    auto ws_idx() const { return ws_w * ws_y + ws_x; }
    auto count_ws(const Json::Value& pos) -> Workspace&;
    auto locate_ws(const Json::Value& geo) -> Workspace&;
    auto locate_ws(const Json::Value& geo) const -> const Workspace&;
  };

  std::unordered_map<std::string, Output> outputs;
  std::unordered_map<size_t, Wset> wsets;
  std::unordered_map<size_t, Json::Value> views;
  std::string focused_output_name;
  size_t maybe_empty_focus_wset_idx = {};
  size_t vswitch_sticky_view_id = {};
  bool new_output_detected = {};
  bool vswitching = {};

  auto update_view(const Json::Value& view) -> void;
};

using Sock = util::ScopedFd;

class IPC : public std::enable_shared_from_this<IPC> {
  static std::weak_ptr<IPC> instance;
  Json::CharReaderBuilder reader_builder;
  Json::StreamWriterBuilder writer_builder;
  std::list<std::pair<std::string, std::reference_wrapper<const EventHandler>>> handlers;
  std::mutex handlers_mutex;
  State state;
  std::mutex state_mutex;

  IPC() = default;

  static auto connect() -> Sock;
  auto receive(Sock& sock) -> Json::Value;
  auto start() -> void;
  auto root_event_handler(const std::string& event, const Json::Value& data) -> void;
  auto update_state_handler(const std::string& event, const Json::Value& data) -> void;

 public:
  static auto get_instance() -> std::shared_ptr<IPC>;
  auto send(const std::string& method, Json::Value&& data) -> Json::Value;
  auto register_handler(const std::string& event, const EventHandler& handler) -> void;
  auto unregister_handler(EventHandler& handler) -> void;

  auto lock_state() -> std::lock_guard<std::mutex> { return std::lock_guard{state_mutex}; }
  auto& get_outputs() const { return state.outputs; }
  auto& get_wsets() const { return state.wsets; }
  auto& get_views() const { return state.views; }
  auto& get_focused_output_name() const { return state.focused_output_name; }
};

}  // namespace waybar::modules::wayfire
