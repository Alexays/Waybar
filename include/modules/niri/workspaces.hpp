#pragma once

#include <gtkmm/box.h>
#include <json/value.h>

#include <memory>
#include <regex>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"
#include "modules/niri/workspace.hpp"
#include "util/regex_collection.hpp"  // Added for rewrite rules

namespace waybar::modules::niri {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const Bar&, const Json::Value&, std::mutex&, std::list<pid_t>&);
  ~Workspaces() override;

  void update() override;

  const Json::Value& config() const { return config_; }
  const Bar& bar() const { return bar_; }

  std::string getIcon(const std::string& value, const Json::Value& ws) const;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();
  void createWorkspace(const Json::Value& workspace_data);
  void sortWorkspaces(std::vector<const Json::Value*>& workspaces) const;
  bool isWorkspaceIgnored(const std::string& name);
  bool handleScroll(GdkEventScroll* /*unused*/) override;
  // Added for window rewrite
  void populateWindowRewriteConfig();
  void populateFormatWindowSeparatorConfig();
  std::string getRewrite(const std::string& app_id, const std::string& title);
  std::string getWindowsRepresentation(const Json::Value& ws);

  const Bar& bar_;
  Gtk::Box box_;

  std::vector<std::unique_ptr<Workspace>> workspaces_;

  // Vec of regex rules to ignore workspaces.
  std::vector<std::regex> ignoreWorkspaces_;

  bool sort_by_id_ = false;
  bool sort_by_name_ = false;
  bool sort_by_coordinates_ = false;

  // Added for window rewrite
  util::RegexCollection m_windowRewriteRules;
  std::string m_windowRewriteDefault;
  std::string m_formatWindowSeparator;
 
};

}  // namespace waybar::modules::niri
