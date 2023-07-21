#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include <string_view>
#include <unordered_map>

#include "AModule.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Workspaces : public AModule, public sigc::trackable {
 public:
  Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~Workspaces() = default;
  auto update() -> void override;

 private:
  static constexpr std::string_view workspace_switch_cmd_ = "workspace {} \"{}\"";
  static constexpr std::string_view persistent_workspace_switch_cmd_ =
      R"(workspace {} "{}"; move workspace to output "{}"; workspace {} "{}")";

  static int convertWorkspaceNameToNum(std::string name);

  void onCmd(const struct Ipc::ipc_response&);
  void onEvent(const struct Ipc::ipc_response&);
  bool filterButtons();
  Gtk::Button& addButton(const Json::Value&);
  void onButtonReady(const Json::Value&, Gtk::Button&);
  std::string getIcon(const std::string&, const Json::Value&);
  const std::string getCycleWorkspace(std::vector<Json::Value>::iterator, bool prev) const;
  uint16_t getWorkspaceIndex(const std::string& name) const;
  std::string trimWorkspaceName(std::string);
  bool handleScroll(GdkEventScroll*) override;

  const Bar& bar_;
  std::vector<Json::Value> workspaces_;
  std::vector<std::string> high_priority_named_;
  std::vector<std::string> workspaces_order_;
  Gtk::Box box_;
  util::JsonParser parser_;
  std::unordered_map<std::string, Gtk::Button> buttons_;
  std::mutex mutex_;
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
