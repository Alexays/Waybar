#pragma once

#include <giomm/dbusconnection.h>
#include <gtkmm/icontheme.h>
#include <libupower-glib/upower.h>

#include <unordered_map>

#include "AIconLabel.hpp"
#include "util/upower_backend.hpp"

using waybar::util::upDevice_output;
using waybar::util::UPowerBackend;

namespace waybar::modules {

class UPower final : public AIconLabel {
 public:
  UPower(const std::string &, const Json::Value &);
  virtual ~UPower();
  auto update() -> void override;

 private:
  const std::string NO_BATTERY{"battery-missing-symbolic"};

  // Config
  bool showIcon_{true};
  bool hideIfEmpty_{true};
  int iconSize_{20};
  int tooltip_spacing_{4};
  int tooltip_padding_{4};
  Gtk::Box contentBox_;  // tooltip box
  std::string tooltipFormat_;

  // Technical variables
  std::string nativePath_;
  std::string model_;
  std::string lastStatus_;
  Glib::ustring label_markup_;
  std::mutex mutex_;
  Glib::RefPtr<Gtk::IconTheme> gtkTheme_;
  bool sleeping_;

  UPowerBackend upower_backend_;
  upDevice_output upDevice_;  // Device to display

  static void deviceNotify_cb(UpDevice *device, GParamSpec *pspec, gpointer user_data);

  void setDisplayDevice();
  const Glib::ustring getText(const upDevice_output &upDevice_, const std::string &format);
  bool queryTooltipCb(int, int, bool, const Glib::RefPtr<Gtk::Tooltip> &);
};

}  // namespace waybar::modules
