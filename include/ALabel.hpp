#pragma once

#include <gtkmm/label.h>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule {
 public:
  ALabel(const Json::Value &, const std::string &, const std::string &, const std::string &format,
         uint16_t interval = 0, bool ellipsize = false, bool enable_click = false,
         bool enable_scroll = false);
  virtual ~ALabel() = default;

 protected:
  Gtk::Label label_;
  const std::chrono::seconds interval_;

  bool handleToggle(GdkEventButton *const &e) override;
  virtual std::string getState(uint8_t value, bool lesser = false);
};

}  // namespace waybar
