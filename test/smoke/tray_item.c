// Fake StatusNotifierItem for tray.sh: registers with the running Waybar and answers
// com.canonical.dbusmenu.GetLayout on /Menu with argv[1] flat entries in one reply.
//
// Build: gcc tray_item.c -o tray_item $(pkg-config --cflags --libs gio-2.0)
#include <gio/gio.h>
#include <stdlib.h>

static const char* introspection =
    "<node>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='GetAll'>"
    "      <arg type='s' direction='in'/><arg type='a{sv}' direction='out'/>"
    "    </method>"
    "  </interface>"
    "  <interface name='com.canonical.dbusmenu'>"
    "    <method name='GetLayout'>"
    "      <arg type='i' direction='in'/><arg type='i' direction='in'/>"
    "      <arg type='as' direction='in'/>"
    "      <arg type='u' direction='out'/><arg type='(ia{sv}av)' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static int n_entries;

static void on_method_call(GDBusConnection* bus, const char* sender, const char* path,
                           const char* interface, const char* method, GVariant* arguments,
                           GDBusMethodInvocation* call, gpointer data) {
  if (g_str_equal(method, "GetAll")) {
    g_dbus_method_invocation_return_value(
        call, g_variant_new_parsed(
                  "({'Id': <'smoke-tray'>, 'Category': <'ApplicationStatus'>, 'Status': <'Active'>,"
                  "  'IconName': <'dialog-information'>, 'Menu': <objectpath '/Menu'>},)"));
    return;
  }

  GVariantBuilder children;
  g_variant_builder_init(&children, G_VARIANT_TYPE("av"));
  for (int id = 1; id <= n_entries; id++) {
    g_variant_builder_add_parsed(&children, "<(%i, {'label': <'entry'>}, @av [])>", id);
  }
  g_dbus_method_invocation_return_value(
      call,
      g_variant_new_parsed("(uint32 1, (0, @a{sv} {}, %@av))", g_variant_builder_end(&children)));
}

int main(int argc, char** argv) {
  static const GDBusInterfaceVTable vtable = {on_method_call};
  GDBusNodeInfo* node = g_dbus_node_info_new_for_xml(introspection, NULL);
  GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  n_entries = argc > 1 ? atoi(argv[1]) : 10;

  g_dbus_connection_register_object(bus, "/StatusNotifierItem", node->interfaces[0], &vtable, NULL,
                                    NULL, NULL);
  g_dbus_connection_register_object(bus, "/Menu", node->interfaces[1], &vtable, NULL, NULL, NULL);
  g_dbus_connection_call_sync(bus, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                              "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
                              g_variant_new("(s)", "/StatusNotifierItem"), NULL, 0, -1, NULL, NULL);
  g_main_loop_run(g_main_loop_new(NULL, FALSE));
  return 0;
}
