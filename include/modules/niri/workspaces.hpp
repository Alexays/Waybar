#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/niri/backend.hpp"
#include "util/RegexCollection.hpp"  // Added for rewrite rules

namespace waybar::modules::niri {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string &, const Bar &, const Json::Value &);
  ~Workspaces() override;
  void update() override;

 private:
  void onEvent(const Json::Value &ev) override;
  void doUpdate();
  Gtk::Button &addButton(const Json::Value &ws);
  std::string getIcon(const std::string &value, const Json::Value &ws);
  // Added for window rewrite
  void populateWindowRewriteConfig();
  void populateFormatWindowSeparatorConfig();
  std::string getRewrite(const std::string &app_id, const std::string &title);

  const Bar &bar_;
  Gtk::Box box_;
  // Map from niri workspace id to button.
  std::unordered_map<uint64_t, Gtk::Button> buttons_;

  // Added for window rewrite
  util::RegexCollection m_windowRewriteRules;
  std::string m_windowRewriteDefault;
  std::string m_formatWindowSeparator;
  std::map<uint64_t, std::vector<std::string>> m_workspaceWindowRepresentations;
};

}  // namespace waybar::modules::niri
