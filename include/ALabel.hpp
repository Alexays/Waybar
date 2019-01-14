#pragma once

#include <json/json.h>
#include "IModule.hpp"
#include <glibmm/markup.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/label.h>

namespace waybar {

class ALabel : public IModule {
public:
  ALabel(const Json::Value &, const std::string format, uint16_t interval = 0);
  virtual ~ALabel() = default;
  virtual auto update() -> void;
  virtual std::string getIcon(uint16_t, const std::string &alt = "");
  virtual operator Gtk::Widget &();

protected:
  Gtk::EventBox event_box_;
  Gtk::Label label_;
  const Json::Value &config_;
  std::string format_;
  std::mutex mutex_;
  const std::chrono::seconds interval_;
  bool alt_ = false;
  std::string default_format_;

private:
  bool handleToggle(GdkEventButton *const &ev);
  bool handleScroll(GdkEventScroll *);
};

} // namespace waybar
