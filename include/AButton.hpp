#pragma once

#include <glibmm/markup.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <json/json.h>

#include "AModule.hpp"

namespace waybar {

class AButton : public AModule {
 public:
  AButton(const Json::Value &, const std::string &, const std::string &, const std::string &format,
          uint16_t interval = 0, bool ellipsize = false, bool enable_click = false,
          bool enable_scroll = false);
  virtual ~AButton() = default;
  virtual auto update() -> void;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);
  virtual std::string getIcon(uint16_t, const std::vector<std::string> &alts, uint16_t max = 0);

 protected:
  Gtk::Button button_ = Gtk::Button(name_);
  Gtk::Label *label_ = (Gtk::Label *)button_.get_child();
  std::string format_;
  const std::chrono::seconds interval_;
  bool alt_ = false;
  std::string default_format_;

  virtual bool handleToggle(GdkEventButton *const &e);
  virtual std::string getState(uint8_t value, bool lesser = false);
};

}  // namespace waybar
