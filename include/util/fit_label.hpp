#pragma once

#include <gtkmm/label.h>
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
 * Gtk::Label that, with fit-height enabled, asks for no more height than one line of its text and
 * scales its font down until all lines fit the height it gets. A multi-line format can then share a
 * horizontal bar with single-line modules without making the bar taller.
 */
class FitLabel : public Gtk::Label {
 public:
  void setFitHeight(bool fit_height) {
    fit_height_ = fit_height;
    queue_resize();
  }

 protected:
  void get_preferred_height_vfunc(int& minimum, int& natural) const override {
    Gtk::Label::get_preferred_height_vfunc(minimum, natural);
    limitToOneLine(minimum, natural);
  }

  void get_preferred_height_for_width_vfunc(int width, int& minimum, int& natural) const override {
    Gtk::Label::get_preferred_height_for_width_vfunc(width, minimum, natural);
    limitToOneLine(minimum, natural);
  }

  void on_size_allocate(Gtk::Allocation& allocation) override {
    Gtk::Label::on_size_allocate(allocation);
    if (!fit_height_) {
      return;
    }
    int text_width = 0;
    int text_height = 0;
    get_layout()->get_pixel_size(text_width, text_height);
    const double scale = fit_scale(scale_, text_height, allocation.get_height() - padding());
    // Ignore rounding noise, so that the resize this causes cannot loop.
    if (std::abs(scale - scale_) < 0.01) {
      return;
    }
    scale_ = scale;
    auto attributes = get_attributes();
    if (attributes.gobj() == nullptr) {
      attributes = Pango::AttrList();
    }
    auto scale_attribute = Pango::Attribute::create_attr_scale(scale_);
    attributes.change(scale_attribute);
    set_attributes(attributes);
  }

 private:
  void limitToOneLine(int& minimum, int& natural) const {
    if (!fit_height_) {
      return;
    }
    const int lines = std::max(1, get_layout()->get_line_count());
    const int text = natural - padding();
    const int one_line =
        padding() + static_cast<int>(std::ceil(static_cast<double>(text) / lines / scale_));
    minimum = std::min(minimum, one_line);
    natural = std::min(natural, one_line);
  }

  // CSS padding and border above and below the text.
  int padding() const {
    auto style = get_style_context();
    const auto state = style->get_state();
    const auto padding = style->get_padding(state);
    const auto border = style->get_border(state);
    return padding.get_top() + padding.get_bottom() + border.get_top() + border.get_bottom();
  }

  bool fit_height_ = false;
  double scale_ = 1.0;
};

}  // namespace waybar::util
