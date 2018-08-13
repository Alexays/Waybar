#include "modules/workspaces.hpp"
#include "ipc/client.hpp"

waybar::modules::Workspaces::Workspaces(Bar &bar)
  : _bar(bar), _scrolling(false)
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
      std::lock_guard<std::mutex> lock(_mutex);
      auto str = ipc_single_command(_ipcfd, IPC_GET_WORKSPACES, nullptr, &len);
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
      std::lock_guard<std::mutex> lock(_mutex);
      auto value = fmt::format("workspace \"{}\"", pair.first->first);
      uint32_t size = value.size();
      ipc_single_command(_ipcfd, IPC_COMMAND, value.c_str(), &size);
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  });
  button.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  button.signal_scroll_event()
    .connect(sigc::mem_fun(*this, &Workspaces::_handleScroll));
  _box.reorder_child(button, node["num"].asInt() - 1);
  if (node["focused"].asBool())
    button.get_style_context()->add_class("current");
  button.show();
}

bool waybar::modules::Workspaces::_handleScroll(GdkEventScroll *e)
{
  std::lock_guard<std::mutex> lock(_mutex);
  if (_scrolling)
    return false;
  _scrolling = true;
  gdouble delta_x, delta_y;
  int nextId = -1;
  for (auto& node : _workspaces)
    if (node["focused"].asBool()) {
      nextId = node["num"].asInt();
      break;
    }
  if (nextId == -1) {
    _scrolling = false;
    return false;
  }
  int id = nextId;
  if (e->direction == GDK_SCROLL_UP || e->direction == GDK_SCROLL_LEFT)
      nextId += 1;
  if (e->direction == GDK_SCROLL_DOWN || e->direction == GDK_SCROLL_RIGHT)
      nextId -= 1;
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdk_event_get_scroll_deltas ((const GdkEvent *) e, &delta_x, &delta_y);
    // TODO: handle delta_y
    if (delta_x < 0)
      nextId += 1;
    else if (delta_x > 0)
      nextId -= 1;
  }
  if (nextId > _workspaces[_workspaces.size() - 1]["num"].asInt())
    nextId = _workspaces[0]["num"].asInt();
  if (nextId < _workspaces[0]["num"].asInt())
    nextId = _workspaces[_workspaces.size() - 1]["num"].asInt();
  if (id == nextId) {
    _scrolling = false;
    return false;
  }
  auto value = fmt::format("workspace \"{}\"", nextId);
  uint32_t size = value.size();
  ipc_single_command(_ipcfd, IPC_COMMAND, value.c_str(), &size);
  _scrolling = false;
  return true;
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
