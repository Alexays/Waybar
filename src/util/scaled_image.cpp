#include "util/scaled_image.hpp"

#include <gdkmm/texture.h>

#include <algorithm>

namespace waybar::util {

void ScaledImage::set_pixbuf(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, int scale) {
  if (!pixbuf) {
    clear();
    width_ = height_ = 0;
    return;
  }
  scale = std::max(1, scale);
  width_ = (pixbuf->get_width() + scale - 1) / scale;
  height_ = (pixbuf->get_height() + scale - 1) / scale;
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  set(Gdk::Texture::create_for_pixbuf(pixbuf));
  G_GNUC_END_IGNORE_DEPRECATIONS
  // The image draws the texture fit into a square of this size
  set_pixel_size(std::max(width_, height_));
}

void ScaledImage::measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum,
                                int& natural, int& minimum_baseline, int& natural_baseline) const {
  if (get_storage_type() != Gtk::Image::Type::PAINTABLE || width_ == 0) {
    Gtk::Image::measure_vfunc(orientation, for_size, minimum, natural, minimum_baseline,
                              natural_baseline);
    return;
  }
  minimum = natural = orientation == Gtk::Orientation::HORIZONTAL ? width_ : height_;
  minimum_baseline = natural_baseline = -1;
}

}  // namespace waybar::util
