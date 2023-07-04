#pragma once

#include <libupower-glib/upower.h>

#include <unordered_map>

#include "gtkmm/box.h"
#include "gtkmm/label.h"
#include "gtkmm/window.h"

namespace waybar::modules::upower {

class UPowerTooltip : public Gtk::Window {
 private:
  typedef std::unordered_map<std::string, UpDevice*> Devices;

  const std::string getDeviceIcon(UpDeviceKind& kind);

  Gtk::Box* contentBox;

  uint iconSize;
  uint tooltipSpacing;
  uint tooltipPadding;

 public:
  UPowerTooltip(uint iconSize, uint tooltipSpacing, uint tooltipPadding);
  virtual ~UPowerTooltip();

  uint updateTooltip(Devices& devices);
};

}  // namespace waybar::modules::upower
