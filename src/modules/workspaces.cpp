#include "modules/workspaces.hpp"
#include "ipc/client.hpp"

waybar::modules::Workspaces::Workspaces(Bar &bar)
  : _bar(bar)
{
  _box.get_style_context()->add_class("workspaces");
  std::string socketPath = get_socketpath();
  _ipcfd = ipc_open_socket(socketPath);
  _ipcEventfd = ipc_open_socket(socketPath);
  const char *subscribe = "[ \"workspace\" ]";
  uint32_t len = strlen(subscribe);
  ipc_single_command(_ipcEventfd, IPC_SUBSCRIBE, subscribe, &len);
  _thread = [this] {
    try {
      if (_bar.outputName.empty()) {
        // Wait for the name of the output
        while (_bar.outputName.empty())
          _thread.sleep_for(chrono::milliseconds(150));
      } else
        ipc_recv_response(_ipcEventfd);
      uint32_t len = 0;
      auto str = ipc_single_command(_ipcfd, IPC_GET_WORKSPACES, nullptr, &len);
      std::lock_guard<std::mutex> lock(_mutex);
      _workspaces = _getWorkspaces(str);
      Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Workspaces::update));
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  };
}

auto waybar::modules::Workspaces::update() -> void
{
  std::lock_guard<std::mutex> lock(_mutex);
  bool needReorder = false;
  for (auto it = _buttons.begin(); it != _buttons.end(); ++it) {
    auto ws = std::find_if(_workspaces.begin(), _workspaces.end(),
      [it](auto node) -> bool { return node["num"].asInt() == it->first; });
    if (ws == _workspaces.end()) {
      it = _buttons.erase(it);
      needReorder = true;
    }
  }
  for (auto node : _workspaces) {
    if (_bar.outputName != node["output"].asString())
      continue;
    auto it = _buttons.find(node["num"].asInt());
    if (it == _buttons.end()) {
      _addWorkspace(node);
      needReorder = true;
    } else {
      auto &button = it->second;
      bool isCurrent = node["focused"].asBool();
      if (!isCurrent) {
        button.get_style_context()->remove_class("current");
      } else if (isCurrent) {
        button.get_style_context()->add_class("current");
      }
      if (needReorder)
        _box.reorder_child(button, node["num"].asInt() - 1);
      button.show();
    }
  }
}

void waybar::modules::Workspaces::_addWorkspace(Json::Value node)
{
  auto pair = _buttons.emplace(node["num"].asInt(), node["name"].asString());
  auto &button = pair.first->second;
  _box.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, pair] {
    try {
      auto value = fmt::format("workspace \"{}\"", pair.first->first);
      uint32_t size = value.size();
      ipc_single_command(_ipcfd, IPC_COMMAND, value.c_str(), &size);
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  });
  _box.reorder_child(button, node["num"].asInt() - 1);
  if (node["focused"].asBool()) {
    button.get_style_context()->add_class("current");
  }
  button.show();
}

Json::Value waybar::modules::Workspaces::_getWorkspaces(const std::string data)
{
  Json::Value res;
  try {
    std::string err;
    res = _parser.parse(data);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  return res;
}

waybar::modules::Workspaces::operator Gtk::Widget &() {
  return _box;
}
