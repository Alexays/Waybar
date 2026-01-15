#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"
#include "util/icon_loader.hpp"

namespace waybar::modules::niri {

class Workspace {
 public:
  Workspace(const Json::Value& config, const uint64_t id, const std::string& name);
  void update(const Json::Value& workspace_data, const std::vector<Json::Value>& windows_data,
              const std::string& display);

  Gtk::Button& button() { return button_; }
  // Gtk::Box& content() { return content_; }
  // Gtk::Label& labelBefore() { return labelBefore_; }
  // Gtk::Label& labelAfter() { return labelAfter_; }

 private:
  std::string getIcon(const std::string& value, const Json::Value& ws);
  void updateTaskbar(const std::vector<Json::Value>& windows_data, const uint64_t active_window_id);

  IconLoader iconLoader_;
  uint64_t id_;
  std::string name_;
  Gtk::Button button_;
  Gtk::Box content_;
  Gtk::Label label_;
  Json::Value config_;
  Json::Value taskBarConfig_;
};

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const Bar&, const Json::Value&);
  ~Workspaces() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();
  void addWorkspace(const Json::Value& workspace_data,
                    const std::vector<Json::Value>& windows_data);

  const Bar& bar_;
  Gtk::Box box_;
  std::unordered_map<uint64_t, std::unique_ptr<Workspace>> workspaces_;
};

}  // namespace waybar::modules::niri
