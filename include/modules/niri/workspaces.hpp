#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include <regex>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"
#include "util/regex_collection.hpp" // Added for rewrite rules

namespace waybar::modules::niri {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const Bar&, const Json::Value&);
  ~Workspaces() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();
  void sortWorkspaces(std::vector<Json::Value>& workspaces) const;
  Gtk::Button& addButton(const Json::Value& ws);
  std::string getIcon(const std::string& value, const Json::Value& ws);
  bool isWorkspaceIgnored(const std::string& name);
  bool handleScroll(GdkEventScroll* /*unused*/) override;
  // Added for window rewrite
  void populateWindowRewriteConfig();
  void populateFormatWindowSeparatorConfig();
  std::string getRewrite(const std::string& app_id, const std::string& title);

  const Bar& bar_;
  Gtk::Box box_;
  // Map from niri workspace id to button.
  std::unordered_map<uint64_t, Gtk::Button> buttons_;
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
