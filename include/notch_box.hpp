#pragma once

#include <gtkmm/container.h>

#include <functional>
#include <utility>
#include <vector>

namespace waybar {

// Container for a bar's modules-right when the bar has a "notch": lays its children out with
// util::notch_layout(). The zone comes from a callback in the toplevel window's coordinates, which
// are also the coordinates of this widget's allocation (it has no GdkWindow of its own).
class NotchBox : public Gtk::Container {
 public:
  using ZoneFn = std::function<std::pair<int, int>()>;  // [start, end) in window coordinates

  explicit NotchBox(ZoneFn zone);
  void setSpacing(int spacing);

 protected:
  void on_add(Gtk::Widget* child) override;
  void on_remove(Gtk::Widget* child) override;
  void forall_vfunc(gboolean include_internals, GtkCallback callback,
                    gpointer callback_data) override;
  GType child_type_vfunc() const override;

  Gtk::SizeRequestMode get_request_mode_vfunc() const override;
  void get_preferred_width_vfunc(int& minimum, int& natural) const override;
  void get_preferred_height_vfunc(int& minimum, int& natural) const override;
  void get_preferred_width_for_height_vfunc(int height, int& minimum, int& natural) const override;
  void get_preferred_height_for_width_vfunc(int width, int& minimum, int& natural) const override;
  void on_size_allocate(Gtk::Allocation& allocation) override;
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

 private:
  Gtk::Border margin() const;
  Gtk::Border frame() const;   // CSS border + padding
  Gtk::Border chrome() const;  // CSS margin + border + padding

  ZoneFn zone_;
  int spacing_ = 0;
  std::vector<Gtk::Widget*> children_;
  // Occupied spans relative to the allocation, so the CSS background is drawn as one block per
  // side of the zone instead of across the whole width.
  std::vector<std::pair<int, int>> segments_;
};

}  // namespace waybar
