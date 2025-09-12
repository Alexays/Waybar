#include "AGraph.hpp"

#include <cairomm/context.h>
#include <fmt/format.h>

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

    drawFilledArea(cr, points, height, bg_color);

    drawLine(cr, points, fg_color);
  }

  return false;
}
void AGraph::drawFilledArea(const Cairo::RefPtr<Cairo::Context>& cr,
                            const std::vector<std::pair<double, double>>& points,
                            double height, const Gdk::RGBA& bg_color) {
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

}  // namespace waybar
