#include "util/fit_label.hpp"

#include <gtk/gtk.h>

namespace waybar::util {

void FitLabel::setFitHeight(bool fit_height) {
  fit_height_ = fit_height;
  queue_resize();
}

void FitLabel::setAngle(unsigned angle) {
  angle_ = angle % 360;
  queue_resize();
}

Gtk::SizeRequestMode FitLabel::get_request_mode_vfunc() const {
  auto mode = Gtk::Label::get_request_mode_vfunc();
  if (!rotated()) {
    return mode;
  }
  switch (mode) {
    case Gtk::SizeRequestMode::HEIGHT_FOR_WIDTH:
      return Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT;
    case Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT:
      return Gtk::SizeRequestMode::HEIGHT_FOR_WIDTH;
    default:
      return mode;
  }
}

void FitLabel::measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
                             int& minimum_baseline, int& natural_baseline) const {
  if (rotated()) {
    Gtk::Label::measure_vfunc(orientation == Gtk::Orientation::HORIZONTAL
                                  ? Gtk::Orientation::VERTICAL
                                  : Gtk::Orientation::HORIZONTAL,
                              for_size, minimum, natural, minimum_baseline, natural_baseline);
    minimum_baseline = natural_baseline = -1;
    return;
  }
  Gtk::Label::measure_vfunc(orientation, for_size, minimum, natural, minimum_baseline,
                            natural_baseline);
  if (fit_height_ && orientation == Gtk::Orientation::VERTICAL) {
    const int lines = std::max(1, get_layout()->get_line_count());
    const int one_line = static_cast<int>(std::ceil(static_cast<double>(natural) / lines / scale_));
    minimum = std::min(minimum, one_line);
    natural = std::min(natural, one_line);
  }
}

void FitLabel::size_allocate_vfunc(int width, int height, int baseline) {
  if (rotated()) {
    // Lays the text out along the height
    Gtk::Label::size_allocate_vfunc(height, width, -1);
    return;
  }
  Gtk::Label::size_allocate_vfunc(width, height, baseline);
  if (!fit_height_) {
    return;
  }
  int text_width = 0;
  int text_height = 0;
  get_layout()->get_pixel_size(text_width, text_height);
  const double scale = fit_scale(scale_, text_height, height);
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

void FitLabel::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) {
  if (angle_ == 0) {
    Gtk::Label::snapshot_vfunc(snapshot);
    return;
  }
  const float width = get_width();
  const float height = get_height();
  auto* snap = snapshot->gobj();
  gtk_snapshot_save(snap);
  if (angle_ == 180) {
    graphene_point_t corner = GRAPHENE_POINT_INIT(width, height);
    gtk_snapshot_translate(snap, &corner);
    gtk_snapshot_rotate(snap, 180);
    Gtk::Label::snapshot_vfunc(snapshot);
    gtk_snapshot_restore(snap);
    return;
  }
  auto layout = get_layout();
  Pango::Rectangle ink;
  Pango::Rectangle logical;
  layout->get_pixel_extents(ink, logical);
  // GTK3 aligned the rotated text box within the widget
  const float box_width = logical.get_height();
  const float box_height = logical.get_width();
  const float x = std::floor(get_xalign() * std::max(0.0F, width - box_width));
  const float y = std::floor(get_yalign() * std::max(0.0F, height - box_height));
  graphene_point_t origin =
      angle_ == 90 ? GRAPHENE_POINT_INIT(x, y + box_height) : GRAPHENE_POINT_INIT(x + box_width, y);
  gtk_snapshot_translate(snap, &origin);
  gtk_snapshot_rotate(snap, angle_ == 90 ? -90 : 90);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_snapshot_render_layout(snap, gtk_widget_get_style_context(GTK_WIDGET(gobj())),
                             -logical.get_x(), -logical.get_y(), layout->gobj());
  G_GNUC_END_IGNORE_DEPRECATIONS
  gtk_snapshot_restore(snap);
}

}  // namespace waybar::util
