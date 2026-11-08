#pragma once

#include <glibmm/extraclassinit.h>
#include <gtkmm/widget.h>

namespace waybar::util {

// Single-child container standing in for the GTK3 Gtk::EventBox that wrapped every module. Its
// CSS node is named "widget" like the EventBox one, so selectors such as
// `.modules-left > widget:first-child` keep matching.
class ModuleBox : public Glib::ExtraClassInit, public Gtk::Widget {
 public:
  ModuleBox();
  ~ModuleBox() override;

  void set_child(Gtk::Widget& child);
  void unset_child();
  Gtk::Widget* get_child();
  const Gtk::Widget* get_child() const;

 protected:
  Gtk::SizeRequestMode get_request_mode_vfunc() const override;
  void measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
                     int& minimum_baseline, int& natural_baseline) const override;
  void size_allocate_vfunc(int width, int height, int baseline) override;
};

}  // namespace waybar::util
