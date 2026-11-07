#pragma once

#include <gtkmm/label.h>
#include <gtkmm/snapshot.h>
#include <pangomm/attributes.h>
#include <pangomm/attrlist.h>

#include <algorithm>
#include <cmath>

namespace waybar::util {

// Font scale that makes text, `text_height` pixels high when drawn at `scale`, fit into
// `available` pixels. The text never grows above its normal size.
inline double fit_scale(double scale, int text_height, int available) {
  if (text_height <= 0) {
    return 1.0;
  }
  if (available <= 0) {
    return scale;
  }
  return std::min(1.0, scale * available / text_height);
}

/*
 * Gtk::Label with two additions:
 * - fit-height: asks for no more height than one line of its text and scales its font down until
 *   all lines fit the height it gets. A multi-line format can then share a horizontal bar with
 *   single-line modules without making the bar taller.
 * - rotation by a multiple of 90 degrees, counterclockwise. Only the text turns, the CSS box does
 *   not, like GtkLabel:angle did in GTK3.
 */
class FitLabel : public Gtk::Label {
 public:
  void setFitHeight(bool fit_height);
  void setAngle(unsigned angle);

 protected:
  Gtk::SizeRequestMode get_request_mode_vfunc() const override;
  void measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
                     int& minimum_baseline, int& natural_baseline) const override;
  void size_allocate_vfunc(int width, int height, int baseline) override;
  void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) override;

 private:
  bool rotated() const { return angle_ == 90 || angle_ == 270; }

  bool fit_height_ = false;
  double scale_ = 1.0;
  unsigned angle_ = 0;
};

}  // namespace waybar::util
