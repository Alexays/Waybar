#pragma once

#include <gtkmm/label.h>
#include <gtkmm/popovermenu.h>

#include <chrono>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule, public Gtk::Widget {
 public:
  virtual ~ALabel();
  auto update() -> void override;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);
  virtual std::string getIcon(uint16_t, const std::vector<std::string> &alts, uint16_t max = 0);
  Gtk::Widget &root() override;

  void setPopupPosition(Gtk::PositionType position) override;

 protected:
  ALabel(const Json::Value &, const std::string &, const std::string &, const std::string &format,
         uint16_t interval = 0, bool ellipsize = false, bool enable_click = false,
         bool enable_scroll = false);

  // Get a reference to the main child.
  // By default returns label_, but a subclass can
  // return a different child, such as a box in AIconLabel
  // WARNING: the field this returns cannot itself be const, since
  // we cast the const away in the non-const version
  virtual Gtk::Widget const &child() const;
  Gtk::Widget &child();

  // Gtk::Widget overrides
  Gtk::SizeRequestMode get_request_mode_vfunc() const override;
  void measure_vfunc(Gtk::Orientation orientation, int for_size, int &minimum, int &natural,
                     int &minimum_baseline, int &natural_baseline) const override;
  void size_allocate_vfunc(int width, int height, int baseline) override;

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
  uint rotate_{0};
};

}  // namespace waybar
