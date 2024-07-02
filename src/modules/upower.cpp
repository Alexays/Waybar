#include "modules/upower.hpp"

#include <giomm/dbuswatchname.h>
#include <gtkmm/tooltip.h>
#include <spdlog/spdlog.h>

namespace waybar::modules {

UPower::UPower(const std::string &id, const Json::Value &config)
    : AIconLabel(config, "upower", id, "{percentage}", 0, true, true, true), sleeping_{false} {
  box_.set_name(name_);
  box_.set_spacing(0);
  box_.set_has_tooltip(AModule::tooltipEnabled());
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

  // Start watching DBUS
  watcherID_ = Gio::DBus::watch_name(
      Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.UPower",
      sigc::mem_fun(*this, &UPower::onAppear), sigc::mem_fun(*this, &UPower::onVanished),
      Gio::DBus::BusNameWatcherFlags::BUS_NAME_WATCHER_FLAGS_AUTO_START);
  // Get DBus async connect
  Gio::DBus::Connection::get(Gio::DBus::BusType::BUS_TYPE_SYSTEM,
                             sigc::mem_fun(*this, &UPower::getConn_cb));

  // Make UPower client
  GError **gErr = NULL;
  upClient_ = up_client_new_full(NULL, gErr);
  if (upClient_ == NULL)
    spdlog::error("Upower. UPower client connection error. {}", (*gErr)->message);

  // Subscribe UPower events
  g_signal_connect(upClient_, "device-added", G_CALLBACK(deviceAdded_cb), this);
  g_signal_connect(upClient_, "device-removed", G_CALLBACK(deviceRemoved_cb), this);

  // Subscribe tooltip query events
  box_.set_has_tooltip();
  box_.signal_query_tooltip().connect(sigc::mem_fun(*this, &UPower::queryTooltipCb), false);

  resetDevices();
  setDisplayDevice();
  // Update the widget
  dp.emit();
}

UPower::~UPower() {
  if (upDevice_.upDevice != NULL) g_object_unref(upDevice_.upDevice);
  if (upClient_ != NULL) g_object_unref(upClient_);
  if (subscrID_ > 0u) {
    conn_->signal_unsubscribe(subscrID_);
    subscrID_ = 0u;
  }
  Gio::DBus::unwatch_name(watcherID_);
  watcherID_ = 0u;
  removeDevices();
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
  if (!upRunning_ || sleeping_) {
    if (hideIfEmpty_) box_.hide();
    return;
  }

  getUpDeviceInfo(upDevice_);

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

  if (devices_.size() == 0 && !upDeviceValid && hideIfEmpty_) {
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

void UPower::getConn_cb(Glib::RefPtr<Gio::AsyncResult> &result) {
  try {
    conn_ = Gio::DBus::Connection::get_finish(result);
    // Subscribe DBUs events
    subscrID_ = conn_->signal_subscribe(sigc::mem_fun(*this, &UPower::prepareForSleep_cb),
                                        "org.freedesktop.login1", "org.freedesktop.login1.Manager",
                                        "PrepareForSleep", "/org/freedesktop/login1");

  } catch (const Glib::Error &e) {
    spdlog::error("Upower. DBus connection error. {}", e.what().c_str());
  }
}

void UPower::onAppear(const Glib::RefPtr<Gio::DBus::Connection> &conn, const Glib::ustring &name,
                      const Glib::ustring &name_owner) {
  upRunning_ = true;
}

void UPower::onVanished(const Glib::RefPtr<Gio::DBus::Connection> &conn,
                        const Glib::ustring &name) {
  upRunning_ = false;
}

void UPower::prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                                const Glib::ustring &sender_name, const Glib::ustring &object_path,
                                const Glib::ustring &interface_name,
                                const Glib::ustring &signal_name,
                                const Glib::VariantContainerBase &parameters) {
  if (parameters.is_of_type(Glib::VariantType("(b)"))) {
    Glib::Variant<bool> sleeping;
    parameters.get_child(sleeping, 0);
    if (!sleeping.get()) {
      resetDevices();
      setDisplayDevice();
      sleeping_ = false;
      // Update the widget
      dp.emit();
    } else
      sleeping_ = true;
  }
}

void UPower::deviceAdded_cb(UpClient *client, UpDevice *device, gpointer data) {
  UPower *up{static_cast<UPower *>(data)};
  up->addDevice(device);
  up->setDisplayDevice();
  // Update the widget
  up->dp.emit();
}

void UPower::deviceRemoved_cb(UpClient *client, const gchar *objectPath, gpointer data) {
  UPower *up{static_cast<UPower *>(data)};
  up->removeDevice(objectPath);
  up->setDisplayDevice();
  // Update the widget
  up->dp.emit();
}

void UPower::deviceNotify_cb(UpDevice *device, GParamSpec *pspec, gpointer data) {
  UPower *up{static_cast<UPower *>(data)};
  // Update the widget
  up->dp.emit();
}

void UPower::addDevice(UpDevice *device) {
  std::lock_guard<std::mutex> guard{mutex_};

  if (G_IS_OBJECT(device)) {
    const gchar *objectPath{up_device_get_object_path(device)};

    // Due to the device getting cleared after this event is fired, we
    // create a new object pointing to its objectPath
    device = up_device_new();
    upDevice_output upDevice{.upDevice = device};
    gboolean ret{up_device_set_object_path_sync(device, objectPath, NULL, NULL)};
    if (!ret) {
      g_object_unref(G_OBJECT(device));
      return;
    }

    if (devices_.find(objectPath) != devices_.cend()) {
      auto upDevice{devices_[objectPath]};
      if (G_IS_OBJECT(upDevice.upDevice)) g_object_unref(upDevice.upDevice);
      devices_.erase(objectPath);
    }

    g_signal_connect(device, "notify", G_CALLBACK(deviceNotify_cb), this);
    devices_.emplace(Devices::value_type(objectPath, upDevice));
  }
}

void UPower::removeDevice(const gchar *objectPath) {
  std::lock_guard<std::mutex> guard{mutex_};
  if (devices_.find(objectPath) != devices_.cend()) {
    auto upDevice{devices_[objectPath]};
    if (G_IS_OBJECT(upDevice.upDevice)) g_object_unref(upDevice.upDevice);
    devices_.erase(objectPath);
  }
}

void UPower::removeDevices() {
  std::lock_guard<std::mutex> guard{mutex_};
  if (!devices_.empty()) {
    auto it{devices_.cbegin()};
    while (it != devices_.cend()) {
      if (G_IS_OBJECT(it->second.upDevice)) g_object_unref(it->second.upDevice);
      devices_.erase(it++);
    }
  }
}

// Removes all devices and adds the current devices
void UPower::resetDevices() {
  // Remove all devices
  removeDevices();

  // Adds all devices
  GPtrArray *newDevices = up_client_get_devices2(upClient_);
  if (newDevices != NULL)
    for (guint i{0}; i < newDevices->len; ++i) {
      UpDevice *device{(UpDevice *)g_ptr_array_index(newDevices, i)};
      if (device && G_IS_OBJECT(device)) addDevice(device);
    }
}

void UPower::setDisplayDevice() {
  std::lock_guard<std::mutex> guard{mutex_};

  if (nativePath_.empty() && model_.empty()) {
    // Unref current upDevice
    if (upDevice_.upDevice != NULL) g_object_unref(upDevice_.upDevice);

    upDevice_.upDevice = up_client_get_display_device(upClient_);
    getUpDeviceInfo(upDevice_);
  } else {
    g_ptr_array_foreach(
        up_client_get_devices2(upClient_),
        [](gpointer data, gpointer user_data) {
          upDevice_output upDevice;
          auto thisPtr{static_cast<UPower *>(user_data)};
          upDevice.upDevice = static_cast<UpDevice *>(data);
          thisPtr->getUpDeviceInfo(upDevice);
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
          // Unref current upDevice
          if (displayDevice.upDevice != NULL) g_object_unref(thisPtr->upDevice_.upDevice);
          // Reassign new upDevice
          thisPtr->upDevice_ = displayDevice;
        },
        this);
  }

  if (upDevice_.upDevice != NULL)
    g_signal_connect(upDevice_.upDevice, "notify", G_CALLBACK(deviceNotify_cb), this);
}

void UPower::getUpDeviceInfo(upDevice_output &upDevice_) {
  if (upDevice_.upDevice != NULL && G_IS_OBJECT(upDevice_.upDevice)) {
    g_object_get(upDevice_.upDevice, "kind", &upDevice_.kind, "state", &upDevice_.state,
                 "percentage", &upDevice_.percentage, "icon-name", &upDevice_.icon_name,
                 "time-to-empty", &upDevice_.time_empty, "time-to-full", &upDevice_.time_full,
                 "temperature", &upDevice_.temperature, "native-path", &upDevice_.nativePath,
                 "model", &upDevice_.model, NULL);
    spdlog::debug(
        "UPower. getUpDeviceInfo. kind: \"{0}\". state: \"{1}\". percentage: \"{2}\". \
icon_name: \"{3}\". time-to-empty: \"{4}\". time-to-full: \"{5}\". temperature: \"{6}\". \
native_path: \"{7}\". model: \"{8}\"",
        fmt::format_int(upDevice_.kind).str(), fmt::format_int(upDevice_.state).str(),
        upDevice_.percentage, upDevice_.icon_name, upDevice_.time_empty, upDevice_.time_full,
        upDevice_.temperature, upDevice_.nativePath, upDevice_.model);
  }
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
  for (auto pairDev : devices_) {
    // Get device info
    getUpDeviceInfo(pairDev.second);

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
