#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include <unordered_map>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/triad/backend.hpp"

namespace waybar::modules::triad {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const Bar&, const Json::Value&);
  ~Workspaces() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();
  Gtk::Button& addButton(const Json::Value& ws);
  std::string getIcon(const std::string& value, const Json::Value& ws);
  static bool isEmpty(const Json::Value& ws);

  const Bar& bar_;
  Gtk::Box box_;
  std::unordered_map<uint64_t, Gtk::Button> buttons_;
};

}  // namespace waybar::modules::triad
