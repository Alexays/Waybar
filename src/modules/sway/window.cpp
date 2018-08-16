#include "modules/sway/window.hpp"
#include "modules/sway/ipc/client.hpp"

waybar::modules::sway::Window::Window(Bar &bar, Json::Value config)
  : _bar(bar), _config(std::move(config))
{
  _label.set_name("window");
  std::string socketPath = getSocketPath();
  _ipcfd = ipcOpenSocket(socketPath);
  _ipcEventfd = ipcOpenSocket(socketPath);
  const char *subscribe = "[ \"window\" ]";
  uint32_t len = strlen(subscribe);
  ipcSingleCommand(_ipcEventfd, IPC_SUBSCRIBE, subscribe, &len);
  _getFocusedWindow();
  _thread = [this] {
    try {
      auto res = ipcRecvResponse(_ipcEventfd);
      auto parsed = _parser.parse(res.payload);
      if ((parsed["change"] == "focus" || parsed["change"] == "title")
        && parsed["container"]["focused"].asBool()) {
        _window = parsed["container"]["name"].asString();
        Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Window::update));
      }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

auto waybar::modules::sway::Window::update() -> void
{
  _label.set_text(_window);
  _label.set_tooltip_text(_window);
}

std::string waybar::modules::sway::Window::_getFocusedNode(Json::Value nodes)
{
  for (auto &node : nodes) {
    if (node["focused"].asBool()) {
      return node["name"].asString();
    }
    auto res = _getFocusedNode(node["nodes"]);
    if (!res.empty()) {
      return res;
    }
  }
  return std::string();
}

void waybar::modules::sway::Window::_getFocusedWindow()
{
  try {
    uint32_t len = 0;
    auto res = ipcSingleCommand(_ipcfd, IPC_GET_TREE, nullptr, &len);
    auto parsed = _parser.parse(res);
    _window = _getFocusedNode(parsed["nodes"]);
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Window::update));
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}

waybar::modules::sway::Window::operator Gtk::Widget &() {
  return _label;
}
