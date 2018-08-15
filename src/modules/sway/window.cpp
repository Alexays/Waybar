#include "modules/sway/window.hpp"
#include "modules/sway/ipc/client.hpp"

waybar::modules::sway::Window::Window(Bar &bar, Json::Value config)
  : _bar(bar), _config(config)
{
  _label.set_name("window");
  std::string socketPath = get_socketpath();
  _ipcfd = ipc_open_socket(socketPath);
  _ipcEventfd = ipc_open_socket(socketPath);
  const char *subscribe = "[ \"window\" ]";
  uint32_t len = strlen(subscribe);
  ipc_single_command(_ipcEventfd, IPC_SUBSCRIBE, subscribe, &len);
  _getFocusedWindow();
  _thread = [this] {
    try {
      if (_bar.outputName.empty()) {
        // Wait for the name of the output
        while (_bar.outputName.empty())
          _thread.sleep_for(chrono::milliseconds(150));
      }
      auto res = ipc_recv_response(_ipcEventfd);
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
    if (node["focused"].asBool())
      return node["name"].asString();
    auto res = _getFocusedNode(node["nodes"]);
    if (!res.empty())
      return res;
  }
  return std::string();
}

void waybar::modules::sway::Window::_getFocusedWindow()
{
  try {
    uint32_t len = 0;
    auto res = ipc_single_command(_ipcfd, IPC_GET_TREE, nullptr, &len);
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
