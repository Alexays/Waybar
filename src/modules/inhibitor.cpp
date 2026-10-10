#include "modules/inhibitor.hpp"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <spdlog/spdlog.h>

namespace {

using DBus = std::unique_ptr<GDBusConnection, void (*)(GDBusConnection*)>;

auto destroyDBus(GDBusConnection* connection) -> void {
  if (!g_dbus_connection_is_closed(connection)) {
    GError* error = nullptr;
    g_dbus_connection_close_sync(connection, nullptr, &error);
    if (error) {
      spdlog::error("g_bus_connection_close_sync failed(): {}", error->message);
      g_error_free(error);
    }
  }
  g_object_unref(connection);
}

// A connection of our own rather than the shared one from g_bus_get_sync(): the
// destructor below closes it, and closing the shared connection would close it
// for the whole process, with g_bus_get_sync() handing the same closed
// connection back to every later caller.
auto dbus() -> DBus {
  GError* error = nullptr;
  gchar* address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);

  if (error) {
    spdlog::error("g_dbus_address_get_for_bus_sync() failed: {}", error->message);
    g_error_free(error);
    return DBus{nullptr, destroyDBus};
  }

  GDBusConnection* connection = g_dbus_connection_new_for_address_sync(
      address,
      static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                        G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
      nullptr, nullptr, &error);
  g_free(address);

  if (error) {
    spdlog::error("g_dbus_connection_new_for_address_sync() failed: {}", error->message);
    g_error_free(error);
    connection = nullptr;
  }

  return DBus{connection, destroyDBus};
}

auto getLocks(const DBus& bus, const std::string& inhibitors) -> int {
  GError* error = nullptr;
  GUnixFDList* fd_list;
  int handle;

  auto reply = g_dbus_connection_call_with_unix_fd_list_sync(
      bus.get(), "org.freedesktop.login1", "/org/freedesktop/login1",
      "org.freedesktop.login1.Manager", "Inhibit",
      g_variant_new("(ssss)", inhibitors.c_str(), "waybar", "Asked by user", "block"),
      G_VARIANT_TYPE("(h)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &fd_list, nullptr, &error);
  if (error) {
    spdlog::error("g_dbus_connection_call_with_unix_fd_list_sync() failed: {}", error->message);
    g_error_free(error);
    handle = -1;
  } else {
    gint index;
    g_variant_get(reply, "(h)", &index);
    g_variant_unref(reply);
    handle = g_unix_fd_list_get(fd_list, index, nullptr);
    g_object_unref(fd_list);
  }

  return handle;
}

auto checkInhibitor(const std::string& inhibitor) -> const std::string& {
  static const auto inhibitors = std::array{"idle",
                                            "shutdown",
                                            "sleep",
                                            "handle-power-key",
                                            "handle-suspend-key",
                                            "handle-hibernate-key",
                                            "handle-lid-switch"};

  if (std::find(inhibitors.begin(), inhibitors.end(), inhibitor) == inhibitors.end()) {
    throw std::runtime_error("invalid logind inhibitor " + inhibitor);
  }

  return inhibitor;
}

auto getInhibitors(const Json::Value& config) -> std::string {
  std::string inhibitors = "idle";

  if (config["what"].empty()) {
    return inhibitors;
  }

  if (config["what"].isString()) {
    return checkInhibitor(config["what"].asString());
  }

  if (config["what"].isArray()) {
    inhibitors = checkInhibitor(config["what"][0].asString());
    for (decltype(config["what"].size()) i = 1; i < config["what"].size(); ++i) {
      inhibitors.append(":");
      inhibitors.append(checkInhibitor(config["what"][i].asString()));
    }
    return inhibitors;
  }

  return inhibitors;
}

}  // namespace

namespace waybar::modules {

Inhibitor::Inhibitor(const std::string& id, const Bar& bar, const Json::Value& config)
    : ALabel(config, "inhibitor", id, "{status}", true),
      dbus_(::dbus()),
      inhibitors_(::getInhibitors(config)) {
  event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &Inhibitor::handleToggle));
}

Inhibitor::~Inhibitor() {
  if (handle_ != -1) {
    ::close(handle_);
  }
}

auto Inhibitor::activated() -> bool { return handle_ != -1; }

auto Inhibitor::update() -> void {
  std::string status_text = activated() ? "activated" : "deactivated";

  label_.get_style_context()->remove_class(activated() ? "deactivated" : "activated");
  label_.set_markup(fmt::format(fmt::runtime(format_), fmt::arg("status", status_text),
                                fmt::arg("icon", getIcon(0, status_text))));
  label_.get_style_context()->add_class(status_text);

  if (tooltipEnabled()) {
    label_.set_tooltip_markup(status_text);
  }

  return ALabel::update();
}

auto Inhibitor::handleToggle(GdkEventButton* const& e) -> bool {
  if (e->button == 1) {
    if (activated()) {
      ::close(handle_);
      handle_ = -1;
    } else {
      if (dbus_ == nullptr || g_dbus_connection_is_closed(dbus_.get())) {
        dbus_ = ::dbus();
      }
      handle_ = dbus_ == nullptr ? -1 : ::getLocks(dbus_, inhibitors_);
      if (handle_ == -1) {
        spdlog::error("cannot get inhibitor locks");
      }
    }
  }

  return ALabel::handleToggle(e);
}

}  // namespace waybar::modules
