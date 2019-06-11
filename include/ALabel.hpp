#pragma once

#include <glibmm/markup.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/label.h>
#include <json/json.h>
#include "IModule.hpp"

namespace waybar {

class ALabel : public IModule {
 public:
  ALabel(const Json::Value &, const std::string &, const std::string &, const std::string &format,
         uint16_t interval = 0);
  virtual ~ALabel();
  virtual auto        update() -> void;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);
  virtual             operator Gtk::Widget &();

 protected:
  bool tooltipEnabled();

  Gtk::EventBox              event_box_;
  Gtk::Label                 label_;
  const Json::Value &        config_;
  std::string                format_;
  std::string                click_param;
  std::mutex                 mutex_;
  const std::chrono::seconds interval_;
  bool                       alt_ = false;
  std::string                default_format_;

  virtual bool        handleToggle(GdkEventButton *const &ev);
  virtual bool        handleScroll(GdkEventScroll *);
  virtual std::string getState(uint8_t value, bool lesser = false);

 private:
  std::vector<int> pid_;
  gdouble distance_scrolled_;
};

}  // namespace waybar
