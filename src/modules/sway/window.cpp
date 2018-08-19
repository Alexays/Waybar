#include "modules/sway/window.hpp"
#include "modules/sway/ipc/client.hpp"

waybar::modules::sway::Window::Window(Bar &bar, Json::Value config)
  : ALabel(std::move(config)), bar_(bar)
{
  label_.set_name("window");
  std::string socketPath = getSocketPath();
  ipcfd_ = ipcOpenSocket(socketPath);
  ipc_eventfd_ = ipcOpenSocket(socketPath);
  ipcSingleCommand(ipc_eventfd_, IPC_SUBSCRIBE, "[ \"window\" ]");
  getFocusedWindow();
  thread_.sig_update.connect(sigc::mem_fun(*this, &Window::update));
  thread_ = [this] {
    try {
      auto res = ipcRecvResponse(ipc_eventfd_);
      auto parsed = parser_.parse(res.payload);
      if ((parsed["change"] == "focus" || parsed["change"] == "title")
        && parsed["container"]["focused"].asBool()) {
        window_ = parsed["container"]["name"].asString();
        thread_.sig_update.emit();
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

waybar::modules::sway::Window::~Window()
{
  close(ipcfd_);
  close(ipc_eventfd_);
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
    auto res = ipcSingleCommand(ipcfd_, IPC_GET_TREE, "");
    auto parsed = parser_.parse(res.payload);
    window_ = getFocusedNode(parsed["nodes"]);
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Window::update));
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
