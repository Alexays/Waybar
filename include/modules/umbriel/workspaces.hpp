#pragma once

#include <gtkmm/box.h>
#include <json/value.h>

#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/umbriel/backend.hpp"
#include "modules/umbriel/workspace.hpp"
#include "util/regex_collection.hpp"

namespace waybar::modules::umbriel {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string& id, const Bar& bar, const Json::Value& config);
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
  bool isWorkspaceIgnored(const std::string& name) const;
  bool handleScroll(GdkEventScroll* /*unused*/) override;

  void populateWindowRewriteConfig();
  void populateFormatWindowSeparatorConfig();
  std::string getRewrite(const std::string& app_id, const std::string& title);
  std::string getWindowsRepresentation(const Json::Value& ws,
                                       const std::vector<Json::Value>& all_windows);

  const Bar& bar_;
  Gtk::Box box_;

  std::vector<std::unique_ptr<Workspace>> workspaces_;

  // Vec of regex rules to ignore workspaces.
  std::vector<std::regex> ignoreWorkspaces_;

  bool sort_by_id_ = false;
  bool sort_by_name_ = false;

  util::RegexCollection m_windowRewriteRules;
  std::string m_windowRewriteDefault;
  std::string m_formatWindowSeparator;
};

}  // namespace waybar::modules::umbriel
