#include "modules/wayfire/backend.hpp"

#include <json/json.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <ranges>
#include <thread>

namespace waybar::modules::wayfire {

std::weak_ptr<IPC> IPC::instance;

// C++23: std::byteswap
inline auto byteswap(uint32_t x) -> uint32_t {
  return (x & 0xff000000) >> 24 | (x & 0x00ff0000) >> 8 | (x & 0x0000ff00) << 8 |
         (x & 0x000000ff) << 24;
}

auto pack_and_write(Sock& sock, std::string&& buf) -> void {
  uint32_t len = buf.size();
  if constexpr (std::endian::native != std::endian::little) len = byteswap(len);
  (void)write(sock.fd, &len, 4);
  (void)write(sock.fd, buf.data(), buf.size());
}

auto read_exact(Sock& sock, size_t n) -> std::string {
  auto buf = std::string(n, 0);
  for (size_t i = 0; i < n;) i += read(sock.fd, &buf[i], n - i);
  return buf;
}

// https://github.com/WayfireWM/pywayfire/blob/69b7c21/wayfire/ipc.py#L438
inline auto is_mapped_toplevel_view(const Json::Value& view) -> bool {
  return view["mapped"].asBool() && view["role"] != "desktop-environment" &&
         view["pid"].asInt() != -1;
}

auto State::Wset::count_ws(const Json::Value& pos) -> Workspace& {
  auto x = pos["x"].asInt();
  auto y = pos["y"].asInt();
  return wss.at(ws_w * y + x);
}

auto State::Wset::locate_ws(const Json::Value& geo) -> Workspace& {
  return const_cast<Workspace&>(std::as_const(*this).locate_ws(geo));
}

auto State::Wset::locate_ws(const Json::Value& geo) const -> const Workspace& {
  const auto& out = output.value().get();
  auto [qx, rx] = std::div(geo["x"].asInt(), out.w);
  auto [qy, ry] = std::div(geo["y"].asInt(), out.h);
  auto x = std::max(0, (int)ws_x + qx - int{rx < 0});
  auto y = std::max(0, (int)ws_y + qy - int{ry < 0});
  return wss.at(ws_w * y + x);
}

auto State::update_view(const Json::Value& view) -> void {
  auto id = view["id"].asUInt();

  // erase old view information
  if (views.contains(id)) {
    auto& old_view = views.at(id);
    auto& ws = wsets.at(old_view["wset-index"].asUInt()).locate_ws(old_view["geometry"]);
    ws.num_views--;
    if (old_view["sticky"].asBool()) ws.num_sticky_views--;
    views.erase(id);
  }

  // insert or assign new view information
  if (is_mapped_toplevel_view(view)) {
    try {
      // view["wset-index"] could be messed up
      auto& ws = wsets.at(view["wset-index"].asUInt()).locate_ws(view["geometry"]);
      ws.num_views++;
      if (view["sticky"].asBool()) ws.num_sticky_views++;
      views.emplace(id, view);
    } catch (const std::exception&) {
    }
  }
}

auto IPC::get_instance() -> std::shared_ptr<IPC> {
  auto p = instance.lock();
  if (!p) instance = p = std::shared_ptr<IPC>(new IPC);
  return p;
}

auto IPC::connect() -> Sock {
  auto* path = std::getenv("WAYFIRE_SOCKET");
  if (path == nullptr) {
    throw std::runtime_error{"Wayfire IPC: ipc not available"};
  }

  auto sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == -1) {
    throw std::runtime_error{"Wayfire IPC: socket() failed"};
  }

  auto addr = sockaddr_un{.sun_family = AF_UNIX};
  std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

  if (::connect(sock, (const sockaddr*)&addr, sizeof(addr)) == -1) {
    close(sock);
    throw std::runtime_error{"Wayfire IPC: connect() failed"};
  }

  return {sock};
}

auto IPC::receive(Sock& sock) -> Json::Value {
  auto len = *reinterpret_cast<uint32_t*>(read_exact(sock, 4).data());
  if constexpr (std::endian::native != std::endian::little) len = byteswap(len);
  auto buf = read_exact(sock, len);

  Json::Value json;
  std::string err;
  auto* reader = reader_builder.newCharReader();
  if (!reader->parse(&*buf.begin(), &*buf.end(), &json, &err)) {
    throw std::runtime_error{"Wayfire IPC: parse json failed: " + err};
  }
  return json;
}

auto IPC::send(const std::string& method, Json::Value&& data) -> Json::Value {
  spdlog::debug("Wayfire IPC: send method \"{}\"", method);
  auto sock = connect();

  Json::Value json;
  json["method"] = method;
  json["data"] = std::move(data);

  pack_and_write(sock, Json::writeString(writer_builder, json));
  auto res = receive(sock);
  root_event_handler(method, res);
  return res;
}

auto IPC::start() -> void {
  spdlog::info("Wayfire IPC: starting");

  // init state
  send("window-rules/list-outputs", {});
  send("window-rules/list-wsets", {});
  send("window-rules/list-views", {});
  send("window-rules/get-focused-view", {});
  send("window-rules/get-focused-output", {});

  std::thread([&] {
    auto sock = connect();

    {
      Json::Value json;
      json["method"] = "window-rules/events/watch";

      pack_and_write(sock, Json::writeString(writer_builder, json));
      if (receive(sock)["result"] != "ok") {
        spdlog::error(
            "Wayfire IPC: method \"window-rules/events/watch\""
            " have failed");
        return;
      }
    }

    while (auto json = receive(sock)) {
      auto ev = json["event"].asString();
      spdlog::debug("Wayfire IPC: received event \"{}\"", ev);
      root_event_handler(ev, json);
    }
  }).detach();
}

auto IPC::register_handler(const std::string& event, const EventHandler& handler) -> void {
  auto _ = std::lock_guard{handlers_mutex};
  handlers.emplace_back(event, handler);
}

auto IPC::unregister_handler(EventHandler& handler) -> void {
  auto _ = std::lock_guard{handlers_mutex};
  handlers.remove_if([&](auto& e) { return &e.second.get() == &handler; });
}

auto IPC::root_event_handler(const std::string& event, const Json::Value& data) -> void {
  bool new_output_detected;
  {
    auto _ = lock_state();
    update_state_handler(event, data);
    new_output_detected = state.new_output_detected;
    state.new_output_detected = false;
  }
  if (new_output_detected) {
    send("window-rules/list-outputs", {});
    send("window-rules/list-wsets", {});
  }
  {
    auto _ = std::lock_guard{handlers_mutex};
    for (const auto& [_event, handler] : handlers)
      if (_event == event) handler(event);
  }
}

auto IPC::update_state_handler(const std::string& event, const Json::Value& data) -> void {
  // IPC events
  // https://github.com/WayfireWM/wayfire/blob/053b222/plugins/ipc-rules/ipc-events.hpp#L108-L125
  /*
    [x] view-mapped
    [x] view-unmapped
    [-] view-set-output  // for detect new output
    [ ] view-geometry-changed  // -> view-workspace-changed
    [x] view-wset-changed
    [x] view-focused
    [x] view-title-changed
    [x] view-app-id-changed
    [x] plugin-activation-state-changed
    [x] output-gain-focus

    [ ] view-tiled
    [ ] view-minimized
    [ ] view-fullscreened
    [x] view-sticky
    [x] view-workspace-changed
    [x] output-wset-changed
    [x] wset-workspace-changed
  */

  if (event == "view-mapped") {
    // data: { event, view }
    state.update_view(data["view"]);
    return;
  }

  if (event == "view-unmapped") {
    // data: { event, view }
    try {
      // data["view"]["wset-index"] could be messed up
      state.update_view(data["view"]);
      state.maybe_empty_focus_wset_idx = data["view"]["wset-index"].asUInt();
    } catch (const std::exception&) {
    }
    return;
  }

  if (event == "view-set-output") {
    // data: { event, output?, view }
    // new output event
    if (!state.outputs.contains(data["view"]["output-name"].asString())) {
      state.new_output_detected = true;
    }
    return;
  }

  if (event == "view-wset-changed") {
    // data: { event, old-wset: wset, new-wset: wset, view }
    state.maybe_empty_focus_wset_idx = data["old-wset"]["index"].asUInt();
    state.update_view(data["view"]);
    return;
  }

  if (event == "view-focused") {
    // data: { event, view? }
    if (const auto& view = data["view"]) {
      try {
        // view["wset-index"] could be messed up
        auto& wset = state.wsets.at(view["wset-index"].asUInt());
        wset.focused_view_id = view["id"].asUInt();
      } catch (const std::exception&) {
      }
    } else {
      // focused to null
      if (state.wsets.contains(state.maybe_empty_focus_wset_idx))
        state.wsets.at(state.maybe_empty_focus_wset_idx).focused_view_id = {};
    }
    return;
  }

  if (event == "view-title-changed" || event == "view-app-id-changed" || event == "view-sticky") {
    // data: { event, view }
    state.update_view(data["view"]);
    return;
  }

  if (event == "plugin-activation-state-changed") {
    // data: { event, plugin: name, state: bool, output: id, output-data: output }
    auto plugin = data["plugin"].asString();
    auto plugin_state = data["state"].asBool();

    if (plugin == "vswitch") {
      state.vswitching = plugin_state;
      if (plugin_state) {
        state.maybe_empty_focus_wset_idx = data["output-data"]["wset-index"].asUInt();
      }
    }

    return;
  }

  if (event == "output-gain-focus") {
    // data: { event, output }
    state.focused_output_name = data["output"]["name"].asString();
    return;
  }

  if (event == "view-workspace-changed") {
    // data: { event, from: point, to: point, view }
    if (state.vswitching) {
      if (state.vswitch_sticky_view_id == 0) {
        auto& wset = state.wsets.at(data["view"]["wset-index"].asUInt());
        auto& old_ws = wset.locate_ws(state.views.at(data["view"]["id"].asUInt())["geometry"]);
        auto& new_ws = wset.count_ws(data["to"]);
        old_ws.num_views--;
        new_ws.num_views++;
        if (data["view"]["sticky"].asBool()) {
          old_ws.num_sticky_views--;
          new_ws.num_sticky_views++;
        }
        state.update_view(data["view"]);
        state.vswitch_sticky_view_id = data["view"]["id"].asUInt();
      } else {
        state.vswitch_sticky_view_id = {};
      }
      return;
    }
    state.update_view(data["view"]);
    return;
  }

  if (event == "output-wset-changed") {
    // data: { event, new-wset: wset.name, output: id, new-wset-data: wset, output-data: output }
    auto& output = state.outputs.at(data["output-data"]["name"].asString());
    auto wset_idx = data["new-wset-data"]["index"].asUInt();
    state.wsets.at(wset_idx).output = output;
    output.wset_idx = wset_idx;
    return;
  }

  if (event == "wset-workspace-changed") {
    // data: { event, previous-workspace: point, new-workspace: point,
    //         output: id, wset: wset.name, output-data: output, wset-data: wset }
    auto wset_idx = data["wset-data"]["index"].asUInt();
    auto& wset = state.wsets.at(wset_idx);
    wset.ws_x = data["new-workspace"]["x"].asUInt();
    wset.ws_y = data["new-workspace"]["y"].asUInt();

    // correct existing views geometry
    auto& out = wset.output.value().get();
    auto dx = (int)out.w * ((int)wset.ws_x - data["previous-workspace"]["x"].asInt());
    auto dy = (int)out.h * ((int)wset.ws_y - data["previous-workspace"]["y"].asInt());
    for (auto& [_, view] : state.views) {
      if (view["wset-index"].asUInt() == wset_idx &&
          view["id"].asUInt() != state.vswitch_sticky_view_id) {
        view["geometry"]["x"] = view["geometry"]["x"].asInt() - dx;
        view["geometry"]["y"] = view["geometry"]["y"].asInt() - dy;
      }
    }
    return;
  }

  // IPC responses
  // https://github.com/WayfireWM/wayfire/blob/053b222/plugins/ipc-rules/ipc-rules.cpp#L27-L37

  if (event == "window-rules/list-views") {
    // data: [ view ]
    state.views.clear();
    for (auto& [_, wset] : state.wsets) std::ranges::fill(wset.wss, State::Workspace{});
    for (const auto& view : data | std::views::filter(is_mapped_toplevel_view)) {
      state.update_view(view);
    }
    return;
  }

  if (event == "window-rules/list-outputs") {
    // data: [ output ]
    state.outputs.clear();
    for (const auto& output_data : data) {
      state.outputs.emplace(output_data["name"].asString(),
                            State::Output{
                                .id = output_data["id"].asUInt(),
                                .w = output_data["geometry"]["width"].asUInt(),
                                .h = output_data["geometry"]["height"].asUInt(),
                                .wset_idx = output_data["wset-index"].asUInt(),
                            });
    }
    return;
  }

  if (event == "window-rules/list-wsets") {
    // data: [ wset ]
    std::unordered_map<size_t, State::Wset> wsets;
    for (const auto& wset_data : data) {
      auto wset_idx = wset_data["index"].asUInt();

      auto output_name = wset_data["output-name"].asString();
      auto output = state.outputs.contains(output_name)
                        ? std::optional{std::ref(state.outputs.at(output_name))}
                        : std::nullopt;

      const auto& ws_data = wset_data["workspace"];
      auto ws_w = ws_data["grid_width"].asUInt();
      auto ws_h = ws_data["grid_height"].asUInt();

      wsets.emplace(wset_idx, State::Wset{
                                  .output = output,
                                  .wss = std::vector<State::Workspace>(ws_w * ws_h),
                                  .ws_w = ws_w,
                                  .ws_h = ws_h,
                                  .ws_x = ws_data["x"].asUInt(),
                                  .ws_y = ws_data["y"].asUInt(),
                              });

      if (state.wsets.contains(wset_idx)) {
        auto& old_wset = state.wsets.at(wset_idx);
        auto& new_wset = wsets.at(wset_idx);
        new_wset.wss = std::move(old_wset.wss);
        new_wset.focused_view_id = old_wset.focused_view_id;
      }
    }
    state.wsets = std::move(wsets);
    return;
  }

  if (event == "window-rules/get-focused-view") {
    // data: { ok, info: view? }
    if (const auto& view = data["info"]) {
      auto& wset = state.wsets.at(view["wset-index"].asUInt());
      wset.focused_view_id = view["id"].asUInt();
      state.update_view(view);
    }
    return;
  }

  if (event == "window-rules/get-focused-output") {
    // data: { ok, info: output }
    state.focused_output_name = data["info"]["name"].asString();
    return;
  }
}

}  // namespace waybar::modules::wayfire
