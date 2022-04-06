#include "modules/upower/upower_tooltip.hpp"

#include "gtkmm/box.h"
#include "gtkmm/enums.h"
#include "gtkmm/icontheme.h"
#include "gtkmm/image.h"
#include "gtkmm/label.h"

namespace waybar::modules::upower {
UPowerTooltip::UPowerTooltip(uint iconSize_, uint tooltipSpacing_, uint tooltipPadding_)
    : Gtk::Window(),
      iconSize(iconSize_),
      tooltipSpacing(tooltipSpacing_),
      tooltipPadding(tooltipPadding_) {
  contentBox = new Gtk::Box(Gtk::ORIENTATION_VERTICAL);

  // Sets the Tooltip Padding
  contentBox->set_margin_top(tooltipPadding);
  contentBox->set_margin_bottom(tooltipPadding);
  contentBox->set_margin_left(tooltipPadding);
  contentBox->set_margin_right(tooltipPadding);

  add(*contentBox);
  contentBox->show();
}

UPowerTooltip::~UPowerTooltip() {}

uint UPowerTooltip::updateTooltip(Devices& devices) {
  // Removes all old devices
  for (auto child : contentBox->get_children()) {
    child->~Widget();
  }

  uint deviceCount = 0;
  // Adds all valid devices
  for (auto pair : devices) {
    UpDevice* device = pair.second;
    std::string objectPath = pair.first;

    if (!G_IS_OBJECT(device)) continue;

    Gtk::Box* box = new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, tooltipSpacing);

    UpDeviceKind kind;
    double percentage;
    gchar* native_path;
    gchar* model;
    gchar* icon_name;

    g_object_get(device, "kind", &kind, "percentage", &percentage, "native-path", &native_path,
                 "model", &model, "icon-name", &icon_name, NULL);

    // Skip Line_Power and BAT0 devices
    if (kind == UP_DEVICE_KIND_LINE_POWER || native_path == NULL || strlen(native_path) == 0 ||
        strcmp(native_path, "BAT0") == 0)
      continue;

    Gtk::Box* modelBox = new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL);
    box->add(*modelBox);
    // Set device icon
    std::string deviceIconName = getDeviceIcon(kind);
    Gtk::Image* deviceIcon = new Gtk::Image();
    deviceIcon->set_pixel_size(iconSize);
    if (!Gtk::IconTheme::get_default()->has_icon(deviceIconName)) {
      deviceIconName = "battery-missing-symbolic";
    }
    deviceIcon->set_from_icon_name(deviceIconName, Gtk::ICON_SIZE_INVALID);
    modelBox->add(*deviceIcon);

    // Set model
    if (model == NULL) model = (gchar*)"";
    Gtk::Label* modelLabel = new Gtk::Label(model);
    modelBox->add(*modelLabel);

    Gtk::Box* chargeBox = new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL);
    box->add(*chargeBox);

    // Set icon
    Gtk::Image* icon = new Gtk::Image();
    icon->set_pixel_size(iconSize);
    if (icon_name == NULL || !Gtk::IconTheme::get_default()->has_icon(icon_name)) {
      icon_name = (char*)"battery-missing-symbolic";
    }
    icon->set_from_icon_name(icon_name, Gtk::ICON_SIZE_INVALID);
    chargeBox->add(*icon);

    // Set percentage
    std::string percentString = std::to_string(int(percentage + 0.5)) + "%";
    Gtk::Label* percentLabel = new Gtk::Label(percentString);
    chargeBox->add(*percentLabel);

    contentBox->add(*box);

    deviceCount++;
  }

  contentBox->show_all();
  return deviceCount;
}

const std::string UPowerTooltip::getDeviceIcon(UpDeviceKind& kind) {
  switch (kind) {
    case UP_DEVICE_KIND_LINE_POWER:
      return "ac-adapter-symbolic";
    case UP_DEVICE_KIND_BATTERY:
      return "battery";
    case UP_DEVICE_KIND_UPS:
      return "uninterruptible-power-supply-symbolic";
    case UP_DEVICE_KIND_MONITOR:
      return "video-display-symbolic";
    case UP_DEVICE_KIND_MOUSE:
      return "input-mouse-symbolic";
    case UP_DEVICE_KIND_KEYBOARD:
      return "input-keyboard-symbolic";
    case UP_DEVICE_KIND_PDA:
      return "pda-symbolic";
    case UP_DEVICE_KIND_PHONE:
      return "phone-symbolic";
    case UP_DEVICE_KIND_MEDIA_PLAYER:
      return "multimedia-player-symbolic";
    case UP_DEVICE_KIND_TABLET:
      return "computer-apple-ipad-symbolic";
    case UP_DEVICE_KIND_COMPUTER:
      return "computer-symbolic";
    case UP_DEVICE_KIND_GAMING_INPUT:
      return "input-gaming-symbolic";
    case UP_DEVICE_KIND_PEN:
      return "input-tablet-symbolic";
    case UP_DEVICE_KIND_TOUCHPAD:
      return "input-touchpad-symbolic";
    case UP_DEVICE_KIND_MODEM:
      return "modem-symbolic";
    case UP_DEVICE_KIND_NETWORK:
      return "network-wired-symbolic";
    case UP_DEVICE_KIND_HEADSET:
      return "audio-headset-symbolic";
    case UP_DEVICE_KIND_HEADPHONES:
      return "audio-headphones-symbolic";
    case UP_DEVICE_KIND_OTHER_AUDIO:
    case UP_DEVICE_KIND_SPEAKERS:
      return "audio-speakers-symbolic";
    case UP_DEVICE_KIND_VIDEO:
      return "camera-web-symbolic";
    case UP_DEVICE_KIND_PRINTER:
      return "printer-symbolic";
    case UP_DEVICE_KIND_SCANNER:
      return "scanner-symbolic";
    case UP_DEVICE_KIND_CAMERA:
      return "camera-photo-symbolic";
    case UP_DEVICE_KIND_BLUETOOTH_GENERIC:
      return "bluetooth-active-symbolic";
    case UP_DEVICE_KIND_TOY:
    case UP_DEVICE_KIND_REMOTE_CONTROL:
    case UP_DEVICE_KIND_WEARABLE:
    case UP_DEVICE_KIND_LAST:
    default:
      return "battery-symbolic";
  }
}
}  // namespace waybar::modules::upower
