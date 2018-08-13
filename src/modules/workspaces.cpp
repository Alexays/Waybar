#include "modules/workspaces.hpp"
#include "ipc/client.hpp"

waybar::modules::Workspaces::Workspaces(Bar &bar)
  : _bar(bar)
{
  _box.get_style_context()->add_class("workspaces");
  std::string socketPath = get_socketpath();
  _ipcSocketfd = ipc_open_socket(socketPath);
  _ipcEventSocketfd = ipc_open_socket(socketPath);
  const char *subscribe = "[ \"workspace\", \"mode\" ]";
  uint32_t len = strlen(subscribe);
  ipc_single_command(_ipcEventSocketfd, IPC_SUBSCRIBE, subscribe, &len);
  _thread = [this] {
    Glib::signal_idle().connect_once(sigc::mem_fun(*this, &Workspaces::update));
    _thread.sleep_for(chrono::milliseconds(250));
  };
}


auto waybar::modules::Workspaces::update() -> void
{
  if (_bar.outputName.empty()) return;
  Json::Value workspaces = _getWorkspaces();
  bool needReorder = false;
  for (auto it = _buttons.begin(); it != _buttons.end(); ++it) {
    auto ws = std::find_if(workspaces.begin(), workspaces.end(),
      [it](auto node) -> bool { return node["num"].asInt() == it->first; });
    if (ws == workspaces.end()) {
      it = _buttons.erase(it);
      needReorder = true;
    }
  }
  for (auto node : workspaces) {
    auto it = _buttons.find(node["num"].asInt());
    if (it == _buttons.end() && _bar.outputName == node["output"].asString()) {
      _addWorkspace(node);
      needReorder = true;
    } else {
      auto styleContext = it->second.get_style_context();
      bool isCurrent = node["focused"].asBool();
      if (!isCurrent) {
        styleContext->remove_class("current");
      } else if (isCurrent) {
        styleContext->add_class("current");
      }
      if (needReorder)
        _box.reorder_child(it->second, node["num"].asInt() - 1);
      it->second.show();
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
      ipc_single_command(_ipcSocketfd, IPC_COMMAND, value.c_str(), &size);
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

Json::Value waybar::modules::Workspaces::_getWorkspaces()
{
  uint32_t len = 0;
  Json::Value root;
  Json::CharReaderBuilder builder;
  Json::CharReader* reader = builder.newCharReader();
  try {
    std::string str = ipc_single_command(_ipcSocketfd, IPC_GET_WORKSPACES,
      nullptr, &len);
    std::string err;
    bool res =
      reader->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
    delete reader;
    if (!res) {
      std::cerr << err << std::endl;
      return root;
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  return root;
}

waybar::modules::Workspaces::operator Gtk::Widget &() {
  return _box;
}
