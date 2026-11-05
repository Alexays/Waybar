#include "util/module_box.hpp"

#include <gtk/gtk.h>

namespace waybar::util {

static void module_box_class_init(void* g_class, void* /*class_data*/) {
  gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(g_class), "widget");
}

ModuleBox::ModuleBox()
    : Glib::ObjectBase("WaybarModuleBox"),
      Glib::ExtraClassInit(module_box_class_init),
      Gtk::Widget() {}

// The content is a member of the module, often destroyed before this box: the GTK tree, not a
// stored pointer, says what is still parented here.
ModuleBox::~ModuleBox() {
  while (auto* widget = gtk_widget_get_first_child(GTK_WIDGET(gobj()))) {
    gtk_widget_unparent(widget);
  }
}

Gtk::Widget* ModuleBox::get_child() {
  auto* widget = gtk_widget_get_first_child(GTK_WIDGET(gobj()));
  return widget != nullptr && !GTK_IS_POPOVER(widget) ? Glib::wrap(widget) : nullptr;
}

const Gtk::Widget* ModuleBox::get_child() const {
  return const_cast<ModuleBox*>(this)->get_child();
}

void ModuleBox::set_child(Gtk::Widget& child) {
  if (get_child() == &child) {
    return;
  }
  unset_child();
  // First, so that menus parented here never take its place
  gtk_widget_insert_after(child.gobj(), GTK_WIDGET(gobj()), nullptr);
}

void ModuleBox::unset_child() {
  if (auto* child = get_child()) {
    child->unparent();
  }
}

Gtk::SizeRequestMode ModuleBox::get_request_mode_vfunc() const {
  const auto* child = get_child();
  return child != nullptr ? child->get_request_mode() : Gtk::SizeRequestMode::CONSTANT_SIZE;
}

void ModuleBox::measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum,
                              int& natural, int& minimum_baseline, int& natural_baseline) const {
  const auto* child = get_child();
  if (child == nullptr || !child->get_visible()) {
    minimum = natural = 0;
    minimum_baseline = natural_baseline = -1;
    return;
  }
  child->measure(orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

void ModuleBox::size_allocate_vfunc(int width, int height, int baseline) {
  for (auto* widget = gtk_widget_get_first_child(GTK_WIDGET(gobj())); widget != nullptr;
       widget = gtk_widget_get_next_sibling(widget)) {
    if (GTK_IS_POPOVER(widget)) {
      // Menus parented to the module follow it when it moves
      if (gtk_widget_get_visible(widget)) {
        gtk_popover_present(GTK_POPOVER(widget));
      }
    } else if (gtk_widget_get_visible(widget)) {
      gtk_widget_allocate(widget, width, height, baseline, nullptr);
    }
  }
}

}  // namespace waybar::util
