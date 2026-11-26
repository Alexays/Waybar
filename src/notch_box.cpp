#include "notch_box.hpp"

#include <gtkmm/stylecontext.h>

#include <algorithm>

#include "util/notch_layout.hpp"

namespace waybar {

NotchBox::NotchBox(ZoneFn zone) : zone_(std::move(zone)) { set_has_window(false); }

void NotchBox::setSpacing(int spacing) {
  spacing_ = std::max(0, spacing);
  queue_resize();
}

void NotchBox::on_add(Gtk::Widget* child) {
  children_.push_back(child);
  child->set_parent(*this);
}

void NotchBox::on_remove(Gtk::Widget* child) {
  auto it = std::find(children_.begin(), children_.end(), child);
  if (it == children_.end()) return;
  const bool was_visible = child->get_visible();
  children_.erase(it);
  child->unparent();
  if (was_visible && get_visible()) queue_resize();
}

void NotchBox::forall_vfunc(gboolean /*include_internals*/, GtkCallback callback,
                            gpointer callback_data) {
  // The callback may remove children (e.g. on destroy), so walk a copy.
  const auto children = children_;
  for (auto* child : children) callback(child->gobj(), callback_data);
}

GType NotchBox::child_type_vfunc() const { return Gtk::Widget::get_type(); }

Gtk::Border NotchBox::margin() const {
  auto ctx = get_style_context();
  return ctx->get_margin(ctx->get_state());
}

Gtk::Border NotchBox::frame() const {
  auto ctx = get_style_context();
  const auto state = ctx->get_state();
  auto pad = ctx->get_padding(state);
  auto border = ctx->get_border(state);
  Gtk::Border sum;
  sum.set_left(pad.get_left() + border.get_left());
  sum.set_right(pad.get_right() + border.get_right());
  sum.set_top(pad.get_top() + border.get_top());
  sum.set_bottom(pad.get_bottom() + border.get_bottom());
  return sum;
}

Gtk::Border NotchBox::chrome() const {
  const auto m = margin();
  const auto f = frame();
  Gtk::Border sum;
  sum.set_left(m.get_left() + f.get_left());
  sum.set_right(m.get_right() + f.get_right());
  sum.set_top(m.get_top() + f.get_top());
  sum.set_bottom(m.get_bottom() + f.get_bottom());
  return sum;
}

Gtk::SizeRequestMode NotchBox::get_request_mode_vfunc() const {
  return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
}

void NotchBox::get_preferred_width_vfunc(int& minimum, int& natural) const {
  const auto c = chrome();
  int sum = 0;
  int count = 0;
  for (auto* child : children_) {
    if (!child->get_visible()) continue;
    int min = 0;
    int nat = 0;
    child->get_preferred_width(min, nat);
    sum += nat;
    count++;
  }
  if (count > 1) sum += spacing_ * (count - 1);
  // Never demand width: what does not fit is hidden instead of growing the bar.
  minimum = c.get_left() + c.get_right();
  natural = minimum + sum;
}

void NotchBox::get_preferred_height_vfunc(int& minimum, int& natural) const {
  const auto c = chrome();
  int min_h = 0;
  int nat_h = 0;
  for (auto* child : children_) {
    if (!child->get_visible()) continue;
    int min = 0;
    int nat = 0;
    child->get_preferred_height(min, nat);
    min_h = std::max(min_h, min);
    nat_h = std::max(nat_h, nat);
  }
  minimum = min_h + c.get_top() + c.get_bottom();
  natural = nat_h + c.get_top() + c.get_bottom();
}

void NotchBox::get_preferred_width_for_height_vfunc(int /*height*/, int& minimum,
                                                    int& natural) const {
  get_preferred_width_vfunc(minimum, natural);
}

void NotchBox::get_preferred_height_for_width_vfunc(int /*width*/, int& minimum,
                                                    int& natural) const {
  get_preferred_height_vfunc(minimum, natural);
}

void NotchBox::on_size_allocate(Gtk::Allocation& allocation) {
  set_allocation(allocation);

  const auto c = chrome();
  const int left = allocation.get_x() + c.get_left();
  const int right = allocation.get_x() + allocation.get_width() - c.get_right();
  const int top = allocation.get_y() + c.get_top();
  const int height = std::max(1, allocation.get_height() - c.get_top() - c.get_bottom());
  const auto [zone_start, zone_end] = zone_ ? zone_() : std::pair<int, int>{0, 0};

  std::vector<Gtk::Widget*> shown;
  std::vector<int> widths;
  for (auto* child : children_) {
    if (!child->get_visible()) continue;
    int min = 0;
    int nat = 0;
    child->get_preferred_width(min, nat);
    shown.push_back(child);
    widths.push_back(nat);
  }

  const auto layout = util::notch_layout(widths, left, right, zone_start, zone_end, spacing_);
  for (size_t i = 0; i < shown.size(); i++) {
    shown[i]->set_child_visible(layout.slots[i].visible);
    if (!layout.slots[i].visible) continue;
    Gtk::Allocation child_alloc(layout.slots[i].x, top, widths[i], height);
    shown[i]->size_allocate(child_alloc);
  }

  // Relative to the allocation and padded back out by border + padding (not margin), for on_draw().
  const auto f = frame();
  segments_.clear();
  for (const auto& [start, end] : layout.segments) {
    segments_.emplace_back(start - f.get_left() - allocation.get_x(),
                           end + f.get_right() - allocation.get_x());
  }
}

bool NotchBox::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
  auto ctx = get_style_context();
  const auto m = margin();
  const int height = std::max(0, get_allocated_height() - m.get_top() - m.get_bottom());
  for (const auto& [start, end] : segments_) {
    if (end <= start || height <= 0) continue;
    ctx->render_background(cr, start, m.get_top(), end - start, height);
    ctx->render_frame(cr, start, m.get_top(), end - start, height);
  }
  return Gtk::Container::on_draw(cr);
}

}  // namespace waybar
