#pragma once

#include <chrono>
#include <gtkmm/label.h>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule, public Gtk::Label {
 public:
  virtual ~ALabel() = default;
  auto update() -> void override;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);
  virtual std::string getIcon(uint16_t, const std::vector<std::string> &alts, uint16_t max = 0);

 protected:
  ALabel(const Json::Value &, const std::string &, const std::string &, const std::string &format,
         uint16_t interval = 0, bool ellipsize = false, bool enable_click = false,
         bool enable_scroll = false);

  std::string format_;
  const std::chrono::seconds interval_;
  bool alt_ = false;
  std::string default_format_;

  void handleToggle(int n_press, double dx, double dy);
  virtual std::string getState(uint8_t value, bool lesser = false);
};

}  // namespace waybar
