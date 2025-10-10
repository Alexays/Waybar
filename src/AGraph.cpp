#include "AGraph.hpp"

#include <cairomm/context.h>
#include <fmt/format.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <util/command.hpp>

#include "config.hpp"

namespace waybar {

AGraph::AGraph(const Json::Value& config, const std::string& name, const std::string& id,
               uint16_t interval, bool enable_click, bool enable_scroll)
    : AModule(config, name, id,
              config["format-alt"].isString() || config["menu"].isString() || enable_click,
              enable_scroll),
      interval_(config_["interval"] == "once"
                    ? std::chrono::seconds::max()
                    : std::chrono::seconds(
                          config_["interval"].isUInt() ? config_["interval"].asUInt() : interval)) {
  graph_.signal_draw().connect(sigc::mem_fun(*this, &AGraph::onDraw));
  graph_.set_name(name);
  if (!id.empty()) {
    graph_.get_style_context()->add_class(id);
  }
  graph_.get_style_context()->add_class(MODULE_CLASS);
  if (config_["width"].isUInt()) {
    graph_.set_size_request(config_["width"].asUInt(), -1);
  } else {
    graph_.set_size_request(100, -1);
  }

  event_box_.add(graph_);

  if (config_["datapoints"].isUInt()) {
    datapoints_ = config_["datapoints_"].asUInt();
  }

  if (config_["y_offset"].isUInt()) {
    y_offset_ = config_["y_offset"].asUInt();
  }

  if (config_["graph_type"].isString()) {
    std::string type = config_["graph_type"].asString();
    if (type == "line") {
      graph_type_ = GraphType::LINE;
    } else if (type == "bar") {
      graph_type_ = GraphType::BAR;
    } else if (type == "gauge") {
      graph_type_ = GraphType::GAUGE;
    }
  }

  // If a GTKMenu is requested in the config
  if (config_["menu"].isString()) {
    // Create the GTKMenu widget
    try {
      // Check that the file exists
      std::string menuFile = config_["menu-file"].asString();

      // there might be "~" or "$HOME" in original path, try to expand it.
      auto result = Config::tryExpandPath(menuFile, "");
      if (result.empty()) {
        throw std::runtime_error("Failed to expand file: " + menuFile);
      }

      menuFile = result.front();
      // Read the menu descriptor file
      std::ifstream file(menuFile);
      if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + menuFile);
      }
      std::stringstream fileContent;
      fileContent << file.rdbuf();
      GtkBuilder* builder = gtk_builder_new();

      // Make the GtkBuilder and check for errors in his parsing
      if (gtk_builder_add_from_string(builder, fileContent.str().c_str(), -1, nullptr) == 0U) {
        throw std::runtime_error("Error found in the file " + menuFile);
      }

      menu_ = gtk_builder_get_object(builder, "menu");
      if (menu_ == nullptr) {
        throw std::runtime_error("Failed to get 'menu' object from GtkBuilder");
      }
      submenus_ = std::map<std::string, GtkMenuItem*>();
      menuActionsMap_ = std::map<std::string, std::string>();

      // Linking actions to the GTKMenu based on
      for (Json::Value::const_iterator it = config_["menu-actions"].begin();
           it != config_["menu-actions"].end(); ++it) {
        std::string key = it.key().asString();
        submenus_[key] = GTK_MENU_ITEM(gtk_builder_get_object(builder, key.c_str()));
        menuActionsMap_[key] = it->asString();
        g_signal_connect(submenus_[key], "activate", G_CALLBACK(handleGtkMenuEvent),
                         (gpointer)menuActionsMap_[key].c_str());
      }
    } catch (std::runtime_error& e) {
      spdlog::warn("Error while creating the menu : {}. Menu popup not activated.", e.what());
    }
  }
}

auto AGraph::update() -> void {
  graph_.queue_draw();
  AModule::update();
}

void AGraph::handleGtkMenuEvent(GtkMenuItem* /*menuitem*/, gpointer data) {
  waybar::util::command::res res = waybar::util::command::exec((char*)data, "GtkMenu");
}

void AGraph::addValue(const int n) {
  if (values_.size() >= datapoints_) {
    values_.pop_front();
  }
  values_.push_back(n);
}

bool AGraph::onDraw(const Cairo::RefPtr<Cairo::Context>& cr) {
  const int width = graph_.get_allocated_width();
  const int height = graph_.get_allocated_height() - 1 - y_offset_;

  if (values_.empty() || width <= 0 || height <= 0) {
    return false;
  }

  auto style_context = graph_.get_style_context();
  Gdk::RGBA fg_color = style_context->get_color(Gtk::STATE_FLAG_NORMAL);
  Gdk::RGBA bg_color = fg_color;
  bg_color.set_alpha(0.3);

  cr->set_line_width(1.0);

  const double step_width = static_cast<double>(width) / datapoints_;
  const int values_count = values_.size();
  const int empty_space = datapoints_ - values_count;

  std::vector<std::pair<double, double>> points;
  points.reserve(values_count);

  for (int i = empty_space; i < datapoints_; ++i) {
    double x = i * step_width;
    int value_index = i - empty_space;
    int value = values_[value_index];
    double y = height - (static_cast<double>(value) / 100.0 * height);
    points.emplace_back(x, y);
  }
  if (!points.empty()) {
    switch (graph_type_) {
      case GraphType::LINE:
        drawFilledArea(cr, points, height, bg_color);
        drawLine(cr, points, fg_color);
        break;
      case GraphType::BAR:
        drawBars(cr, width, height, values_.empty() ? 0 : values_.back(), fg_color);
        break;
      case GraphType::GAUGE:
        drawGauge(cr, width, height, values_.empty() ? 0 : values_.back(), fg_color);
        break;
    }
  }

  return false;
}
void AGraph::drawFilledArea(const Cairo::RefPtr<Cairo::Context>& cr,
                            const std::vector<std::pair<double, double>>& points, double height,
                            const Gdk::RGBA& bg_color) {
  if (points.empty()) return;

  double first_x = points.front().first;
  double last_x = points.back().first;

  drawPath(cr, points);

  cr->line_to(last_x, height);
  cr->line_to(first_x, height);
  cr->close_path();

  cr->set_source_rgba(bg_color.get_red(), bg_color.get_green(), bg_color.get_blue(),
                      bg_color.get_alpha());
  cr->fill();
}

void AGraph::drawLine(const Cairo::RefPtr<Cairo::Context>& cr,
                      const std::vector<std::pair<double, double>>& points,
                      const Gdk::RGBA& fg_color) {
  if (points.empty()) return;

  cr->begin_new_path();
  drawPath(cr, points);

  cr->set_source_rgba(fg_color.get_red(), fg_color.get_green(), fg_color.get_blue(),
                      fg_color.get_alpha());
  cr->stroke();
}

void AGraph::drawPath(const Cairo::RefPtr<Cairo::Context>& cr,
                      const std::vector<std::pair<double, double>>& points) {
  if (points.empty()) return;

  bool first_point = true;
  for (const auto& point : points) {
    if (first_point) {
      cr->move_to(point.first, point.second);
      first_point = false;
    } else {
      cr->line_to(point.first, point.second);
    }
  }
}

void AGraph::drawBars(const Cairo::RefPtr<Cairo::Context>& cr,
                      double width, double height, int current_value,
                      const Gdk::RGBA& fg_color) {

  current_value = std::min(100, std::max(0, current_value));

  double green_height = height * (std::min(current_value, 40) / 100.0);
  cr->set_source_rgba(0.0, 1.0, 0.0, 1.0);
  cr->rectangle(0, height - green_height, width, green_height);
  cr->fill();

  if (current_value > 40) {
    double yellow_height = height * (std::min(current_value, 75) - 40) / 100.0;
    cr->set_source_rgba(1.0, 1.0, 0.0, 1.0);
    cr->rectangle(0, height - green_height - yellow_height, width, yellow_height);
    cr->fill();
  }

  if (current_value > 75) {
    double orange_height = height * (std::min(current_value, 85) - 75) / 100.0;
    cr->set_source_rgba(1.0, 0.5, 0.0, 1.0);
    double yellow_height = height * (std::min(current_value, 75) - 40) / 100.0;
    cr->rectangle(0, height - green_height - yellow_height - orange_height, width,
                  orange_height);
    cr->fill();
  }

  if (current_value > 85) {
    double red_height = height * (current_value - 85) / 100.0;
    cr->set_source_rgba(1.0, 0.0, 0.0, 1.0);
    double yellow_height = height * (std::min(current_value, 75) - 40) / 100.0;
    double orange_height = height * (std::min(current_value, 85) - 75) / 100.0;
    cr->rectangle(0, height - green_height - yellow_height - orange_height - red_height, width,
                  red_height);
    cr->fill();
  }

  double value_height = height * (current_value / 100.0);
  cr->set_source_rgba(0.2, 0.2, 0.2, 0.8);
  cr->rectangle(0, height - value_height, width, 2);
  cr->fill();
}

void AGraph::drawGauge(const Cairo::RefPtr<Cairo::Context>& cr, double width, double height,
                       int current_value, const Gdk::RGBA& fg_color) {
  double center_x = width / 2.0;
  double center_y = height;
  double radius = height / 2.0;

  cr->set_line_width(10.0);

  double angle1 = M_PI;
  double angle2 = angle1 + 0.3 * angle1;

  // Green section (0-33%)
  cr->set_source_rgba(0.0, 1.0, 0.0, 1.0);
  cr->arc(center_x, center_y, radius, angle1, angle2);
  cr->stroke();

  // Yellow section (33-66%)
  angle1 = angle2;
  angle2 = angle1 + 0.3 * angle1;
  cr->set_source_rgba(1.0, 1.0, 0.0, 1.0);
  cr->arc(center_x, center_y, radius, angle1, angle2);
  cr->stroke();

  // Red section (66-100%)
  angle1 = angle2;
  angle2 = 0.0;
  cr->set_source_rgba(1.0, 0.0, 0.0, 0.8);
  cr->arc(center_x, center_y, radius, angle1, angle2);
  cr->stroke();

  // Draw needle
  double percentage = std::min(100, std::max(0, current_value)) / 100.0;
  double needle_angle = M_PI * percentage;
  double needle_length = radius;

  double needle_x = center_x - needle_length * cos(needle_angle);
  double needle_y = center_y - needle_length * sin(needle_angle);

  cr->set_source_rgba(1.0, 1.0, 1.0, 1.0);
  cr->set_line_width(2.0);
  cr->begin_new_path();
  cr->move_to(center_x, center_y);
  cr->line_to(needle_x, needle_y);
  cr->stroke();
}

}  // namespace waybar
