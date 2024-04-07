#pragma once

#include <libupower-glib/upower.h>

#include <memory>
#include <unordered_map>

#include "gtkmm/box.h"
#include "gtkmm/label.h"
#include "gtkmm/window.h"

namespace wabar::modules::upower {

class UPowerTooltip : public Gtk::Window {
 private:
  typedef std::unordered_map<std::string, UpDevice*> Devices;

  const std::string getDeviceIcon(UpDeviceKind& kind);

  std::unique_ptr<Gtk::Box> contentBox;

  uint iconSize;
  uint tooltipSpacing;
  uint tooltipPadding;

 public:
  UPowerTooltip(uint iconSize, uint tooltipSpacing, uint tooltipPadding);
  virtual ~UPowerTooltip();

  uint updateTooltip(Devices& devices);
};

}  // namespace wabar::modules::upower
