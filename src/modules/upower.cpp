#include "modules/upower.hpp"

#include <giomm/dbuswatchname.h>
#include <glibmm/variant.h>
#include <gtkmm/tooltip.h>
#include <spdlog/spdlog.h>

#include "util/string.hpp"

namespace waybar::modules {

static constexpr const char* UPOWER_NAME{"org.freedesktop.UPower"};
static constexpr const char* UPOWER_PATH{"/org/freedesktop/UPower"};
static constexpr const char* DISPLAY_DEVICE_PATH{"/org/freedesktop/UPower/devices/DisplayDevice"};

UPower::UPower(const std::string& id, const Json::Value& config)
    : AIconLabel(config, "upower", id, "{percentage}", 0, true, true, true), sleeping_{false} {
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

  // Start watching DBUS
  watcherID_ = Gio::DBus::watch_name(
      Gio::DBus::BusType::BUS_TYPE_SYSTEM, "org.freedesktop.UPower",
      sigc::mem_fun(*this, &UPower::onAppear), sigc::mem_fun(*this, &UPower::onVanished),
      Gio::DBus::BusNameWatcherFlags::BUS_NAME_WATCHER_FLAGS_AUTO_START);
  // Devices are requested once the connection is up: nothing here may wait for upowerd, which
  // rules out UpClient since it blocks the main thread whenever a device is added
  Gio::DBus::Connection::get(Gio::DBus::BusType::BUS_TYPE_SYSTEM,
                             sigc::mem_fun(*this, &UPower::getConn_cb));

  // Subscribe tooltip query events
  box_.set_has_tooltip(AModule::tooltipEnabled());
  if (AModule::tooltipEnabled()) {
    box_.signal_query_tooltip().connect(sigc::mem_fun(*this, &UPower::queryTooltipCb), false);
  }
}

UPower::~UPower() {
  removeDevices();
  if (upDevice_.upDevice != NULL) {
    g_signal_handlers_disconnect_by_data(upDevice_.upDevice, this);
    g_object_unref(upDevice_.upDevice);
    upDevice_.upDevice = NULL;
  }
  if (subscrID_ > 0u) {
    conn_->signal_unsubscribe(subscrID_);
    subscrID_ = 0u;
  }
  if (subscrAddedID_ > 0u) {
    conn_->signal_unsubscribe(subscrAddedID_);
    subscrAddedID_ = 0u;
  }
  if (subscrRemovedID_ > 0u) {
    conn_->signal_unsubscribe(subscrRemovedID_);
    subscrRemovedID_ = 0u;
  }
  Gio::DBus::unwatch_name(watcherID_);
  watcherID_ = 0u;
}

static std::string_view getDeviceStatus(UpDeviceState& state) {
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

static std::string_view getDeviceIcon(UpDeviceKind& kind) {
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
  if (!box_.get_style_context()->has_class(std::string(status)))
    box_.get_style_context()->add_class(std::string(status));
  lastStatus_ = status;

  if (devices_.size() == 0 && !upDeviceValid && hideIfEmpty_) {
    box_.hide();
    // Call parent update
    AModule::update();
    return;
  }

  setLabelMarkup(getText(upDevice_, format_));
  // Set icon
  if (upDevice_.icon_name.empty() || !gtkTheme_->has_icon(upDevice_.icon_name))
    upDevice_.icon_name = NO_BATTERY;
  image_.set_from_icon_name(upDevice_.icon_name, Gtk::ICON_SIZE_INVALID);

  box_.show();

  // Call parent update
  ALabel::update();
}

void UPower::getConn_cb(Glib::RefPtr<Gio::AsyncResult>& result) {
  try {
    conn_ = Gio::DBus::Connection::get_finish(result);
    // Subscribe DBUs events
    subscrID_ = conn_->signal_subscribe(sigc::mem_fun(*this, &UPower::prepareForSleep_cb),
                                        "org.freedesktop.login1", "org.freedesktop.login1.Manager",
                                        "PrepareForSleep", "/org/freedesktop/login1");
    subscrAddedID_ = conn_->signal_subscribe(sigc::mem_fun(*this, &UPower::deviceAdded_cb),
                                             UPOWER_NAME, UPOWER_NAME, "DeviceAdded", UPOWER_PATH);
    subscrRemovedID_ =
        conn_->signal_subscribe(sigc::mem_fun(*this, &UPower::deviceRemoved_cb), UPOWER_NAME,
                                UPOWER_NAME, "DeviceRemoved", UPOWER_PATH);
    resetDevices();
  } catch (const Glib::Error& e) {
    spdlog::error("Upower. DBus connection error. {}", e.what().c_str());
  }
}

void UPower::onAppear(const Glib::RefPtr<Gio::DBus::Connection>& conn, const Glib::ustring& name,
                      const Glib::ustring& name_owner) {
  upRunning_ = true;
}

void UPower::onVanished(const Glib::RefPtr<Gio::DBus::Connection>& conn,
                        const Glib::ustring& name) {
  upRunning_ = false;
}

void UPower::prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                                const Glib::ustring& sender_name, const Glib::ustring& object_path,
                                const Glib::ustring& interface_name,
                                const Glib::ustring& signal_name,
                                const Glib::VariantContainerBase& parameters) {
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

void UPower::enumerateDevices_cb(Glib::RefPtr<Gio::AsyncResult>& result) {
  try {
    const auto reply{conn_->call_finish(result)};
    if (reply.is_of_type(Glib::VariantType("(ao)"))) {
      Glib::Variant<std::vector<Glib::DBusObjectPathString>> objectPaths;
      reply.get_child(objectPaths, 0);
      for (const auto& objectPath : objectPaths.get()) loadDevice(objectPath);
    }
  } catch (const Glib::Error& e) {
    spdlog::error("Upower. Enumerate devices error. {}", e.what().c_str());
  }
}

void UPower::deviceAdded_cb(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                            const Glib::ustring& sender_name, const Glib::ustring& object_path,
                            const Glib::ustring& interface_name, const Glib::ustring& signal_name,
                            const Glib::VariantContainerBase& parameters) {
  if (parameters.is_of_type(Glib::VariantType("(o)"))) {
    Glib::Variant<Glib::DBusObjectPathString> objectPath;
    parameters.get_child(objectPath, 0);
    loadDevice(objectPath.get());
  }
}

void UPower::deviceRemoved_cb(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                              const Glib::ustring& sender_name, const Glib::ustring& object_path,
                              const Glib::ustring& interface_name, const Glib::ustring& signal_name,
                              const Glib::VariantContainerBase& parameters) {
  if (parameters.is_of_type(Glib::VariantType("(o)"))) {
    Glib::Variant<Glib::DBusObjectPathString> objectPath;
    parameters.get_child(objectPath, 0);
    removeDevice(objectPath.get().c_str());
    setDisplayDevice();
    // Update the widget
    dp.emit();
  }
}

void UPower::deviceLoaded_cb(GObject* source, GAsyncResult* result, gpointer data) {
  GTask* task{G_TASK(result)};
  // The device was removed or requested anew, or the module is gone
  if (g_cancellable_is_cancelled(g_task_get_cancellable(task))) return;

  UPower* up{static_cast<UPower*>(data)};
  const std::string objectPath{static_cast<const gchar*>(g_task_get_task_data(task))};
  up->loading_.erase(objectPath);

  GError* gErr = NULL;
  UpDevice* device{static_cast<UpDevice*>(g_task_propagate_pointer(task, &gErr))};
  if (device == NULL) {
    spdlog::error("Upower. Device {} error. {}", objectPath,
                  gErr ? gErr->message : "unknown error");
    if (gErr) g_error_free(gErr);
    return;
  }

  if (objectPath == DISPLAY_DEVICE_PATH) {
    std::lock_guard<std::mutex> guard{up->mutex_};
    if (up->upDevice_.upDevice != NULL) {
      g_signal_handlers_disconnect_by_data(up->upDevice_.upDevice, up);
      g_object_unref(up->upDevice_.upDevice);
    }
    up->upDevice_.upDevice = device;
    g_signal_connect(device, "notify", G_CALLBACK(deviceNotify_cb), up);
  } else {
    up->addDevice(device);
    up->setDisplayDevice();
  }
  // Update the widget
  up->dp.emit();
}

void UPower::deviceNotify_cb(UpDevice* device, GParamSpec* pspec, gpointer data) {
  UPower* up{static_cast<UPower*>(data)};
  // Update the widget
  up->dp.emit();
}

// libupower only builds a device with a blocking call, so that runs in a thread, as
// up_client_get_devices_async() does; deviceLoaded_cb() takes the result on the main thread
void UPower::loadDevice(const std::string& objectPath) {
  auto& cancellable{loading_[objectPath]};
  if (cancellable) cancellable->cancel();
  cancellable = Gio::Cancellable::create();

  GTask* task{g_task_new(NULL, cancellable->gobj(), deviceLoaded_cb, this)};
  g_task_set_task_data(task, g_strdup(objectPath.c_str()), g_free);
  g_task_run_in_thread(
      task, [](GTask* task, gpointer source, gpointer objectPath, GCancellable* cancellable) {
        GError* gErr = NULL;
        UpDevice* device{up_device_new()};
        if (up_device_set_object_path_sync(device, static_cast<const gchar*>(objectPath),
                                           cancellable, &gErr)) {
          g_task_return_pointer(task, device, g_object_unref);
        } else {
          g_object_unref(device);
          g_task_return_error(task, gErr);
        }
      });
  g_object_unref(task);
}

// Takes over the caller's reference
void UPower::addDevice(UpDevice* device) {
  std::lock_guard<std::mutex> guard{mutex_};

  if (G_IS_OBJECT(device)) {
    const gchar* objectPath{up_device_get_object_path(device)};
    upDevice_output upDevice{.upDevice = device};

    if (devices_.find(objectPath) != devices_.cend()) {
      auto upDevice{devices_[objectPath]};
      if (G_IS_OBJECT(upDevice.upDevice)) {
        g_signal_handlers_disconnect_by_data(upDevice.upDevice, this);
        g_object_unref(upDevice.upDevice);
      }
      devices_.erase(objectPath);
    }

    g_signal_connect(device, "notify", G_CALLBACK(deviceNotify_cb), this);
    devices_.emplace(Devices::value_type(objectPath, upDevice));
  }
}

void UPower::removeDevice(const gchar* objectPath) {
  std::lock_guard<std::mutex> guard{mutex_};
  if (auto it{loading_.find(objectPath)}; it != loading_.cend()) {
    it->second->cancel();
    loading_.erase(it);
  }
  if (devices_.find(objectPath) != devices_.cend()) {
    auto upDevice{devices_[objectPath]};
    if (G_IS_OBJECT(upDevice.upDevice)) {
      g_signal_handlers_disconnect_by_data(upDevice.upDevice, this);
      g_object_unref(upDevice.upDevice);
    }
    devices_.erase(objectPath);
  }
}

void UPower::removeDevices() {
  std::lock_guard<std::mutex> guard{mutex_};
  for (const auto& pairLoad : loading_) pairLoad.second->cancel();
  loading_.clear();
  if (!devices_.empty()) {
    auto it{devices_.cbegin()};
    while (it != devices_.cend()) {
      if (G_IS_OBJECT(it->second.upDevice)) {
        g_signal_handlers_disconnect_by_data(it->second.upDevice, this);
        g_object_unref(it->second.upDevice);
      }
      devices_.erase(it++);
    }
  }
}

// Removes all devices and requests the current ones
void UPower::resetDevices() {
  removeDevices();

  conn_->call(UPOWER_PATH, UPOWER_NAME, "EnumerateDevices", Glib::VariantContainerBase(),
              sigc::mem_fun(*this, &UPower::enumerateDevices_cb), UPOWER_NAME);
  if (nativePath_.empty() && model_.empty()) loadDevice(DISPLAY_DEVICE_PATH);
}

void UPower::setDisplayDevice() {
  std::lock_guard<std::mutex> guard{mutex_};

  // UPower's own display device is loaded by resetDevices()
  if (nativePath_.empty() && model_.empty()) return;

  // The object belongs to devices_, which owns its notify handler
  if (upDevice_.upDevice != NULL) {
    g_object_unref(upDevice_.upDevice);
    upDevice_.upDevice = NULL;
  }

  for (const auto& pairDev : devices_) {
    upDevice_output upDevice{.upDevice = pairDev.second.upDevice};
    getUpDeviceInfo(upDevice);
    if (nativePath_.empty() ? upDevice.model == model_ : upDevice.nativePath == nativePath_) {
      upDevice_ = upDevice;
    }
  }

  // devices_ owns its elements, so keep a reference of our own
  if (upDevice_.upDevice != NULL) g_object_ref(upDevice_.upDevice);
}

void UPower::getUpDeviceInfo(upDevice_output& upDevice_) {
  if (upDevice_.upDevice != NULL && G_IS_OBJECT(upDevice_.upDevice)) {
    // g_object_get() returns newly allocated copies of the string properties
    gchar* icon_name{NULL};
    gchar* nativePath{NULL};
    gchar* model{NULL};
    g_object_get(upDevice_.upDevice, "kind", &upDevice_.kind, "state", &upDevice_.state,
                 "percentage", &upDevice_.percentage, "icon-name", &icon_name, "time-to-empty",
                 &upDevice_.time_empty, "time-to-full", &upDevice_.time_full, "temperature",
                 &upDevice_.temperature, "native-path", &nativePath, "model", &model, NULL);
    upDevice_.icon_name = (icon_name != NULL) ? icon_name : "";
    upDevice_.nativePath = (nativePath != NULL) ? nativePath : "";
    upDevice_.model = (model != NULL) ? model : "";
    g_free(icon_name);
    g_free(nativePath);
    g_free(model);
    spdlog::debug(
        "UPower. getUpDeviceInfo. kind: \"{0}\". state: \"{1}\". percentage: \"{2}\". \
icon_name: \"{3}\". time-to-empty: \"{4}\". time-to-full: \"{5}\". temperature: \"{6}\". \
native_path: \"{7}\". model: \"{8}\"",
        fmt::format_int(upDevice_.kind).str(), fmt::format_int(upDevice_.state).str(),
        upDevice_.percentage, upDevice_.icon_name, upDevice_.time_empty, upDevice_.time_full,
        upDevice_.temperature, upDevice_.nativePath, upDevice_.model);
  }
}

const Glib::ustring UPower::getText(const upDevice_output& upDevice_, const std::string& format) {
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
        fmt::arg("model", Glib::Markup::escape_text(toSingleLine(upDevice_.model)).raw()),
        fmt::arg("native-path", Glib::Markup::escape_text(upDevice_.nativePath).raw()));
  }

  return ret;
}

bool UPower::queryTooltipCb(int x, int y, bool keyboard_tooltip,
                            const Glib::RefPtr<Gtk::Tooltip>& tooltip) {
  std::lock_guard<std::mutex> guard{mutex_};

  // remove() only unparents a record; deleting it also destroys its managed children
  for (auto* child : contentBox_.get_children()) {
    contentBox_.remove(*child);
    delete child;
  }

  // Fill content box with the content
  for (auto pairDev : devices_) {
    // Get device info
    getUpDeviceInfo(pairDev.second);

    if (pairDev.second.kind != UpDeviceKind::UP_DEVICE_KIND_UNKNOWN &&
        pairDev.second.kind != UpDeviceKind::UP_DEVICE_KIND_LINE_POWER) {
      // Make box record
      Gtk::Box* boxRec{Gtk::make_managed<Gtk::Box>(box_.get_orientation(), tooltip_spacing_)};
      contentBox_.add(*boxRec);
      Gtk::Box* boxDev{Gtk::make_managed<Gtk::Box>(box_.get_orientation())};
      Gtk::Box* boxUsr{Gtk::make_managed<Gtk::Box>(box_.get_orientation())};
      boxRec->add(*boxDev);
      boxRec->add(*boxUsr);
      // Construct device box
      // Set icon from kind
      std::string iconNameDev{getDeviceIcon(pairDev.second.kind)};
      if (!gtkTheme_->has_icon(iconNameDev)) iconNameDev = (char*)NO_BATTERY.c_str();
      Gtk::Image* iconDev{Gtk::make_managed<Gtk::Image>()};
      iconDev->set_from_icon_name(iconNameDev, Gtk::ICON_SIZE_INVALID);
      iconDev->set_pixel_size(iconSize_);
      boxDev->add(*iconDev);
      // Set label from model
      Gtk::Label* labelDev{Gtk::make_managed<Gtk::Label>(pairDev.second.model)};
      // The model comes from the device (a Bluetooth name); keep it within what GDK can draw
      labelDev->set_max_width_chars(50);
      labelDev->set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
      labelDev->set_single_line_mode(true);
      boxDev->add(*labelDev);
      // Construct user box
      // Set icon from icon state
      if (pairDev.second.icon_name.empty() || !gtkTheme_->has_icon(pairDev.second.icon_name))
        pairDev.second.icon_name = NO_BATTERY;
      Gtk::Image* iconTooltip{Gtk::make_managed<Gtk::Image>()};
      iconTooltip->set_from_icon_name(pairDev.second.icon_name, Gtk::ICON_SIZE_INVALID);
      iconTooltip->set_pixel_size(iconSize_);
      boxUsr->add(*iconTooltip);
      // Set markup text
      Gtk::Label* labelTooltip{Gtk::make_managed<Gtk::Label>()};
      labelTooltip->set_markup(getText(pairDev.second, tooltipFormat_));
      boxUsr->add(*labelTooltip);
    }
  }
  tooltip->set_custom(contentBox_);
  contentBox_.show_all();

  return true;
}

}  // namespace waybar::modules
