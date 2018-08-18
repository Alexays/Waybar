#include "modules/sway/window.hpp"
#include "modules/sway/ipc/client.hpp"

waybar::modules::sway::Window::Window(Bar &bar, Json::Value config)
  : ALabel(std::move(config)), bar_(bar)
{
  label_.set_name("window");
  std::string socketPath = getSocketPath();
  ipcfd_ = ipcOpenSocket(socketPath);
  ipc_eventfd_ = ipcOpenSocket(socketPath);
  const char *subscribe = "[ \"window\" ]";
  uint32_t len = strlen(subscribe);
  ipcSingleCommand(ipc_eventfd_, IPC_SUBSCRIBE, subscribe, &len);
  getFocusedWindow();
  thread_ = [this] {
    try {
      auto res = ipcRecvResponse(ipc_eventfd_);
      auto parsed = parser_.parse(res.payload);
      if ((parsed["change"] == "focus" || parsed["change"] == "title")
        && parsed["container"]["focused"].asBool()) {
        window_ = parsed["container"]["name"].asString();
        Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Window::update));
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

auto waybar::modules::sway::Window::update() -> void
{
  label_.set_text(window_);
  label_.set_tooltip_text(window_);
}

std::string waybar::modules::sway::Window::getFocusedNode(Json::Value nodes)
{
  for (auto &node : nodes) {
    if (node["focused"].asBool()) {
      return node["name"].asString();
    }
    auto res = getFocusedNode(node["nodes"]);
    if (!res.empty()) {
      return res;
    }
  }
  return std::string();
}

void waybar::modules::sway::Window::getFocusedWindow()
{
  try {
    uint32_t len = 0;
    auto res = ipcSingleCommand(ipcfd_, IPC_GET_TREE, nullptr, &len);
    auto parsed = parser_.parse(res);
    window_ = getFocusedNode(parsed["nodes"]);
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Window::update));
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
