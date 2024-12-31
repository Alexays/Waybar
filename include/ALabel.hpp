#pragma once

#include <gtkmm/label.h>
#include <gtkmm/popovermenu.h>

#include <chrono>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule {
 public:
  virtual ~ALabel() = default;
  auto update() -> void override;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);
  virtual std::string getIcon(uint16_t, const std::vector<std::string> &alts, uint16_t max = 0);
  Gtk::Widget &root() override;

 protected:
  ALabel(const Json::Value &, const std::string &, const std::string &, const std::string &format,
         uint16_t interval = 0, bool ellipsize = false, bool enable_click = false,
         bool enable_scroll = false);

  Gtk::Label label_;
  std::string format_;
  const std::chrono::seconds interval_;
  bool alt_ = false;
  std::string default_format_;
  std::unique_ptr<Gtk::PopoverMenu> menu_;

  void handleToggle(int n_press, double dx, double dy) override;
  virtual std::string getState(uint8_t value, bool lesser = false);
  void handleClick(const std::string &name) override;

 private:
  void handleMenu(std::string cmd) const;
};

}  // namespace waybar
