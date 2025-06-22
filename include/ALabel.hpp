#pragma once

#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <json/json.h>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule {
 public:
  ALabel(const Json::Value &, const std::string &, const std::string &, const std::string &format,
         uint16_t interval = 0, bool ellipsize = false, bool enable_click = false,
         bool enable_scroll = false);
  virtual ~ALabel() = default;
  auto update() -> void override;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);
  virtual std::string getIcon(uint16_t, const std::vector<std::string> &alts, uint16_t max = 0);

 protected:
  Gtk::Label label_;
  std::string format_;
  const std::chrono::seconds interval_;
  bool alt_ = false;
  std::string default_format_;

  bool handleToggle(GdkEventButton *const &e) override;
  virtual std::string getState(uint8_t value, bool lesser = false);

  std::map<std::string, GtkMenuItem *> submenus_;
  std::map<std::string, std::string> menuActionsMap_;
  static void handleGtkMenuEvent(GtkMenuItem *menuitem, gpointer data);
};

}  // namespace waybar
