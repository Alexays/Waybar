#pragma once

#include <gdkmm/pixbuf.h>
#include <gtkmm/image.h>

namespace waybar::util {

// Gtk::Image that shows a pixbuf of `scale` device pixels per logical pixel at its logical size,
// like a GTK3 cairo surface with a device scale did. A GTK4 image alone is always square.
class ScaledImage : public Gtk::Image {
 public:
  void set_pixbuf(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, int scale);

 protected:
  void measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
                     int& minimum_baseline, int& natural_baseline) const override;

 private:
  int width_ = 0;
  int height_ = 0;
};

}  // namespace waybar::util
