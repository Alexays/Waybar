#include "modules/workspaces.hpp"
#include "ipc/client.hpp"

waybar::modules::Workspaces::Workspaces(Bar &bar)
  : _bar(bar), _thread(nullptr), _box(Gtk::manage(new Gtk::Box))
{
  _box->get_style_context()->add_class("workspaces");
  std::string socketPath = get_socketpath();
  _ipcSocketfd = ipc_open_socket(socketPath);
  _ipcEventSocketfd = ipc_open_socket(socketPath);
  const char *subscribe = "[ \"workspace\", \"mode\" ]";
  uint32_t len = strlen(subscribe);
  ipc_single_command(_ipcEventSocketfd, IPC_SUBSCRIBE, subscribe, &len);
  _idle_timer =
		org_kde_kwin_idle_get_idle_timeout(_bar.client.idle_manager,
      _bar.client.seat, 10000); // 10 seconds
  static const struct org_kde_kwin_idle_timeout_listener idle_timer_listener = {
    .idle = _handle_idle,
    .resumed = _handle_resume,
  };
  org_kde_kwin_idle_timeout_add_listener(_idle_timer,
    &idle_timer_listener, this);
  _updateThread();
}

auto waybar::modules::Workspaces::update() -> void
{
  Json::Value workspaces = _getWorkspaces();
  bool hided = false;
  for (auto it = _buttons.begin(); it != _buttons.end(); ++it) {
    auto ws = std::find_if(workspaces.begin(), workspaces.end(),
      [it](auto node) -> bool { return node["num"].asInt() == it->first; });
    if (ws == workspaces.end()) {
      it->second.hide();
      hided = true;
    }
  }
  for (auto node : workspaces) {
    auto it = _buttons.find(node["num"].asInt());
    if (it == _buttons.end()) {
      _addWorkspace(node);
    } else {
      auto styleContext = it->second.get_style_context();
      bool isCurrent = node["focused"].asBool();
      if (styleContext->has_class("current") && !isCurrent) {
        styleContext->remove_class("current");
      } else if (!styleContext->has_class("current") && isCurrent) {
        styleContext->add_class("current");
      }
      if (hided) {
        _box->reorder_child(it->second, node["num"].asInt() - 1);
      }
      it->second.show();
    }
  }
}

void waybar::modules::Workspaces::_updateThread()
{
  _thread = new waybar::util::SleeperThread([this] {
    update();
    _thread->sleep_for(waybar::chrono::milliseconds(250));
  });
}

void waybar::modules::Workspaces::_handle_idle(void *data,
  struct org_kde_kwin_idle_timeout *timer) {
	auto o = reinterpret_cast<waybar::modules::Workspaces *>(data);
  if (o->_thread) {
	  delete o->_thread;
    o->_thread = nullptr;
    std::cout << "IDLE" << std::endl;
  }
}

void waybar::modules::Workspaces::_handle_resume(void *data,
  struct org_kde_kwin_idle_timeout *timer) {
	auto o = reinterpret_cast<waybar::modules::Workspaces *>(data);
  if (!o->_thread) {
	  o->_updateThread();
    std::cout << "RESUME" << std::endl;
  }
}

void waybar::modules::Workspaces::_addWorkspace(Json::Value node)
{
  auto pair = _buttons.emplace(node["num"].asInt(), node["name"].asString());
  auto &button = pair.first->second;
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, pair] {
    auto value = fmt::format("workspace \"{}\"", pair.first->first);
    uint32_t size = value.size();
    ipc_single_command(_ipcSocketfd, IPC_COMMAND, value.c_str(), &size);
  });
  _box->pack_start(button, false, false, 0);
  _box->reorder_child(button, node["num"].asInt() - 1);
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
  std::string err;
  std::string str = ipc_single_command(_ipcSocketfd, IPC_GET_WORKSPACES,
    nullptr, &len);
  bool res = reader->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
  delete reader;
  if (!res) {
    std::cerr << err << std::endl;
    return nullptr;
  }
  return root;
}

waybar::modules::Workspaces::operator Gtk::Widget &() {
  return *_box;
}
