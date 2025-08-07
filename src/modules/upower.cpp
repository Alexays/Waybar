#include "modules/upower.hpp"

#include <giomm/dbuswatchname.h>
#include <gtkmm/tooltip.h>
#include <spdlog/spdlog.h>

#include "util/upower_backend.hpp"

using waybar::util::upDevice_output;

namespace waybar::modules {

UPower::UPower(const std::string &id, const Json::Value &config)
    : AIconLabel(config, "upower", id, "{percentage}", 0, true, true, true),
      sleeping_{false},
      upower_backend_([this](bool devices_changed) {
        if (devices_changed) {
          setDisplayDevice();
        }
        dp.emit();
      }) {
  box_.set_name(name_);
  box_.set_spacing(0);
  // Tooltip box
  contentBox_.set_orientation((box_.get_orientation() == Gtk::ORIENTATION_HORIZONTAL)
                                  ? Gtk::ORIENTATION_VERTICAL
                                  : Gtk::ORIENTATION_HORIZONTAL);
  // Get current theme
  gtkTheme_ = Gtk::IconTheme::get_default();

  // Icon Size
  if (config_["icon-size"].isInt()) {
    iconSize_ = config_["icon-size"].asInt();
  }
  image_.set_pixel_size(iconSize_);

  // Show icon only when "show-icon" isn't set to false
  if (config_["show-icon"].isBool()) showIcon_ = config_["show-icon"].asBool();
  if (!showIcon_) box_.remove(image_);
  // Device user wants
  if (config_["native-path"].isString()) nativePath_ = config_["native-path"].asString();
  // Device model user wants
  if (config_["model"].isString()) model_ = config_["model"].asString();

  // Hide If Empty
  if (config_["hide-if-empty"].isBool()) hideIfEmpty_ = config_["hide-if-empty"].asBool();

  // Tooltip Spacing
  if (config_["tooltip-spacing"].isInt()) tooltip_spacing_ = config_["tooltip-spacing"].asInt();

  // Tooltip Padding
  if (config_["tooltip-padding"].isInt()) {
    tooltip_padding_ = config_["tooltip-padding"].asInt();
    contentBox_.set_margin_top(tooltip_padding_);
    contentBox_.set_margin_bottom(tooltip_padding_);
    contentBox_.set_margin_left(tooltip_padding_);
    contentBox_.set_margin_right(tooltip_padding_);
  }

  // Tooltip Format
  if (config_["tooltip-format"].isString()) tooltipFormat_ = config_["tooltip-format"].asString();

  // Subscribe tooltip query events
  box_.set_has_tooltip(AModule::tooltipEnabled());
  if (AModule::tooltipEnabled()) {
    box_.signal_query_tooltip().connect(sigc::mem_fun(*this, &UPower::queryTooltipCb), false);
  }

  setDisplayDevice();
  // Update the widget
  dp.emit();
}

UPower::~UPower() {
  if (upDevice_.upDevice != NULL) g_object_unref(upDevice_.upDevice);
}

static const std::string getDeviceStatus(UpDeviceState &state) {
  switch (state) {
    case UP_DEVICE_STATE_CHARGING:
    case UP_DEVICE_STATE_PENDING_CHARGE:
      return "charging";
    case UP_DEVICE_STATE_DISCHARGING:
    case UP_DEVICE_STATE_PENDING_DISCHARGE:
      return "discharging";
    case UP_DEVICE_STATE_FULLY_CHARGED:
      return "full";
    case UP_DEVICE_STATE_EMPTY:
      return "empty";
    default:
      return "unknown-status";
  }
}

static const std::string getDeviceIcon(UpDeviceKind &kind) {
  switch (kind) {
    case UP_DEVICE_KIND_LINE_POWER:
      return "ac-adapter-symbolic";
    case UP_DEVICE_KIND_BATTERY:
      return "battery-symbolic";
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

static std::string secondsToString(const std::chrono::seconds sec) {
  const auto ds{std::chrono::duration_cast<std::chrono::days>(sec)};
  const auto hrs{std::chrono::duration_cast<std::chrono::hours>(sec - ds)};
  const auto min{std::chrono::duration_cast<std::chrono::minutes>(sec - ds - hrs)};
  std::string_view strRet{(ds.count() > 0)    ? "{D}d {H}h {M}min"
                          : (hrs.count() > 0) ? "{H}h {M}min"
                          : (min.count() > 0) ? "{M}min"
                                              : ""};
  spdlog::debug(
      "UPower::secondsToString(). seconds: \"{0}\", minutes: \"{1}\", hours: \"{2}\", \
days: \"{3}\", strRet: \"{4}\"",
      sec.count(), min.count(), hrs.count(), ds.count(), strRet);
  return fmt::format(fmt::runtime(strRet), fmt::arg("D", ds.count()), fmt::arg("H", hrs.count()),
                     fmt::arg("M", min.count()));
}

auto UPower::update() -> void {
  std::lock_guard<std::mutex> guard{mutex_};
  // Don't update widget if the UPower service isn't running
  if (!upower_backend_.running() || sleeping_) {
    if (hideIfEmpty_) box_.hide();
    return;
  }

  upower_backend_.getUpDeviceInfo(upDevice_);

  if (upDevice_.upDevice == NULL && hideIfEmpty_) {
    box_.hide();
    return;
  }
  /* Every Device which is handled by Upower and which is not
   * UP_DEVICE_KIND_UNKNOWN (0) or UP_DEVICE_KIND_LINE_POWER (1) is a Battery
   */
  const bool upDeviceValid{upDevice_.kind != UpDeviceKind::UP_DEVICE_KIND_UNKNOWN &&
                           upDevice_.kind != UpDeviceKind::UP_DEVICE_KIND_LINE_POWER};
  // Get CSS status
  const auto status{getDeviceStatus(upDevice_.state)};
  // Remove last status if it exists
  if (!lastStatus_.empty() && box_.get_style_context()->has_class(lastStatus_))
    box_.get_style_context()->remove_class(lastStatus_);
  if (!box_.get_style_context()->has_class(status)) box_.get_style_context()->add_class(status);
  lastStatus_ = status;

  if (upower_backend_.devices().size() == 0 && !upDeviceValid && hideIfEmpty_) {
    box_.hide();
    // Call parent update
    AModule::update();
    return;
  }

  label_.set_markup(getText(upDevice_, format_));
  // Set icon
  if (upDevice_.icon_name == NULL || !gtkTheme_->has_icon(upDevice_.icon_name))
    upDevice_.icon_name = (char *)NO_BATTERY.c_str();
  image_.set_from_icon_name(upDevice_.icon_name, Gtk::ICON_SIZE_INVALID);

  box_.show();

  // Call parent update
  ALabel::update();
}

void UPower::setDisplayDevice() {
  std::lock_guard<std::mutex> guard{mutex_};

  if (upDevice_.upDevice != NULL) {
    g_object_unref(upDevice_.upDevice);
    upDevice_.upDevice = NULL;
  }

  if (nativePath_.empty() && model_.empty()) {
    upDevice_.upDevice = up_client_get_display_device(upower_backend_.client());
    upower_backend_.getUpDeviceInfo(upDevice_);
  } else {
    g_ptr_array_foreach(
        up_client_get_devices2(upower_backend_.client()),
        [](gpointer data, gpointer user_data) {
          upDevice_output upDevice;
          auto thisPtr{static_cast<UPower *>(user_data)};
          upDevice.upDevice = static_cast<UpDevice *>(data);
          thisPtr->upower_backend_.getUpDeviceInfo(upDevice);
          upDevice_output displayDevice{NULL};
          if (!thisPtr->nativePath_.empty()) {
            if (upDevice.nativePath == nullptr) return;
            if (0 == std::strcmp(upDevice.nativePath, thisPtr->nativePath_.c_str())) {
              displayDevice = upDevice;
            }
          } else {
            if (upDevice.model == nullptr) return;
            if (0 == std::strcmp(upDevice.model, thisPtr->model_.c_str())) {
              displayDevice = upDevice;
            }
          }
          // Unref current upDevice if it exists
          if (displayDevice.upDevice != NULL) {
            thisPtr->upDevice_ = displayDevice;
          }
        },
        this);
  }

  if (upDevice_.upDevice != NULL)
    g_signal_connect(upDevice_.upDevice, "notify", G_CALLBACK(deviceNotify_cb), this);
}

void UPower::deviceNotify_cb(UpDevice *device, GParamSpec *pspec, gpointer data) {
  UPower *up{static_cast<UPower *>(data)};
  // Update the widget
  up->dp.emit();
}

const Glib::ustring UPower::getText(const upDevice_output &upDevice_, const std::string &format) {
  Glib::ustring ret{""};
  if (upDevice_.upDevice != NULL) {
    std::string timeStr{""};
    switch (upDevice_.state) {
      case UP_DEVICE_STATE_CHARGING:
      case UP_DEVICE_STATE_PENDING_CHARGE:
        timeStr = secondsToString(std::chrono::seconds(upDevice_.time_full));
        break;
      case UP_DEVICE_STATE_DISCHARGING:
      case UP_DEVICE_STATE_PENDING_DISCHARGE:
        timeStr = secondsToString(std::chrono::seconds(upDevice_.time_empty));
        break;
      default:
        break;
    }

    ret = fmt::format(
        fmt::runtime(format),
        fmt::arg("percentage", std::to_string((int)std::round(upDevice_.percentage)) + '%'),
        fmt::arg("time", timeStr),
        fmt::arg("temperature", fmt::format("{:-.2g}C", upDevice_.temperature)),
        fmt::arg("model", upDevice_.model), fmt::arg("native-path", upDevice_.nativePath));
  }

  return ret;
}

bool UPower::queryTooltipCb(int x, int y, bool keyboard_tooltip,
                            const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
  std::lock_guard<std::mutex> guard{mutex_};

  // Clear content box
  contentBox_.forall([this](Gtk::Widget &wg) { contentBox_.remove(wg); });

  // Fill content box with the content
  for (auto pairDev : upower_backend_.devices()) {
    // Get device info
    upower_backend_.getUpDeviceInfo(pairDev.second);

    if (pairDev.second.kind != UpDeviceKind::UP_DEVICE_KIND_UNKNOWN &&
        pairDev.second.kind != UpDeviceKind::UP_DEVICE_KIND_LINE_POWER) {
      // Make box record
      Gtk::Box *boxRec{new Gtk::Box{box_.get_orientation(), tooltip_spacing_}};
      contentBox_.add(*boxRec);
      Gtk::Box *boxDev{new Gtk::Box{box_.get_orientation()}};
      Gtk::Box *boxUsr{new Gtk::Box{box_.get_orientation()}};
      boxRec->add(*boxDev);
      boxRec->add(*boxUsr);
      // Construct device box
      // Set icon from kind
      std::string iconNameDev{getDeviceIcon(pairDev.second.kind)};
      if (!gtkTheme_->has_icon(iconNameDev)) iconNameDev = (char *)NO_BATTERY.c_str();
      Gtk::Image *iconDev{new Gtk::Image{}};
      iconDev->set_from_icon_name(iconNameDev, Gtk::ICON_SIZE_INVALID);
      iconDev->set_pixel_size(iconSize_);
      boxDev->add(*iconDev);
      // Set label from model
      Gtk::Label *labelDev{new Gtk::Label{pairDev.second.model}};
      boxDev->add(*labelDev);
      // Construct user box
      // Set icon from icon state
      if (pairDev.second.icon_name == NULL || !gtkTheme_->has_icon(pairDev.second.icon_name))
        pairDev.second.icon_name = (char *)NO_BATTERY.c_str();
      Gtk::Image *iconTooltip{new Gtk::Image{}};
      iconTooltip->set_from_icon_name(pairDev.second.icon_name, Gtk::ICON_SIZE_INVALID);
      iconTooltip->set_pixel_size(iconSize_);
      boxUsr->add(*iconTooltip);
      // Set markup text
      Gtk::Label *labelTooltip{new Gtk::Label{}};
      labelTooltip->set_markup(getText(pairDev.second, tooltipFormat_));
      boxUsr->add(*labelTooltip);
    }
  }
  tooltip->set_custom(contentBox_);
  contentBox_.show_all();

  return true;
}

}  // namespace waybar::modules
