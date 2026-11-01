#pragma once

#include <gtkmm/button.h>
#include <json/value.h>

#include <unordered_map>
#include <unordered_set>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/mango/backend.hpp"

namespace waybar::modules::mango {

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const Bar&, const Json::Value&);
  ~Workspaces() override;
  void update() override;

 private:
  void onEvent(const Json::Value& ev) override;
  void doUpdate();
  bool isTagVisible(uint64_t idx) const;

  Gtk::Button& addButton(uint64_t idx);
  void updateButtonState(Gtk::Button& button, const Json::Value& tag, const Json::Value& monitor);
  std::string getIcon(const std::string& value, const Json::Value& tag);
  bool handleButtonClick(GdkEventButton* event, uint64_t idx, bool isOverview);

  const Bar& bar_;
  Gtk::Box box_;

  std::unordered_map<uint64_t, Gtk::Button> buttons_;
  std::unordered_set<uint64_t> tag_filter_;
  Gtk::Button* overview_button_ = nullptr;

  std::string on_click_left_;
  std::string on_click_middle_;
  std::string on_click_right_;
};

}  // namespace waybar::modules::mango
