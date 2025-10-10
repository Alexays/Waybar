#pragma once

#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <json/json.h>

#include <deque>
#include <vector>

#include "AModule.hpp"

namespace waybar {

enum class GraphType { LINE, BAR, GAUGE };

class AGraph : public AModule {
 public:
  AGraph(const Json::Value &, const std::string &, const std::string &, uint16_t interval = 0,
         bool enable_click = false, bool enable_scroll = false);
  virtual ~AGraph() = default;
  auto update() -> void override;

 protected:
  Gtk::DrawingArea graph_;
  std::deque<int> values_;
  uint16_t datapoints_ = 20;
  uint16_t y_offset_ = 0;
  GraphType graph_type_ = GraphType::LINE;

  void addValue(const int n);

  const std::chrono::seconds interval_;

  bool onDraw(const Cairo::RefPtr<Cairo::Context> &cr);

  std::map<std::string, GtkMenuItem *> submenus_;
  std::map<std::string, std::string> menuActionsMap_;
  static void handleGtkMenuEvent(GtkMenuItem *menuitem, gpointer data);

 private:
  void drawFilledArea(const Cairo::RefPtr<Cairo::Context> &cr,
                      const std::vector<std::pair<double, double>> &points, double height,
                      const Gdk::RGBA &bg_color);

  void drawLine(const Cairo::RefPtr<Cairo::Context> &cr,
                const std::vector<std::pair<double, double>> &points, const Gdk::RGBA &fg_color);

  void drawPath(const Cairo::RefPtr<Cairo::Context> &cr,
                const std::vector<std::pair<double, double>> &points);

  void drawBars(const Cairo::RefPtr<Cairo::Context> &cr,
                double width, double height, int current_value,
                const Gdk::RGBA &fg_color);

  void drawGauge(const Cairo::RefPtr<Cairo::Context> &cr, double width, double height,
                 int current_value, const Gdk::RGBA &fg_color);
};

}  // namespace waybar
