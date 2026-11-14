// Stand-in portal for portal.sh: owns the portal's bus name and emits one
// SettingChanged signal per argument, each a GVariant in text form.
#include <gio/gio.h>
#include <stdio.h>

int main(int argc, char** argv) {
  GError* err = NULL;
  GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
  if (bus == NULL) {
    fprintf(stderr, "portal_peer: %s\n", err->message);
    return 1;
  }
  GVariant* reply = g_dbus_connection_call_sync(
      bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
      g_variant_new("(su)", "org.freedesktop.portal.Desktop", 0), G_VARIANT_TYPE("(u)"),
      G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
  if (reply == NULL) {
    fprintf(stderr, "portal_peer: RequestName: %s\n", err->message);
    return 1;
  }
  guint32 result;
  g_variant_get(reply, "(u)", &result);
  g_variant_unref(reply);
  if (result != 1 /* DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER */) {
    fprintf(stderr, "portal_peer: could not own the portal name (%u)\n", result);
    return 1;
  }
  // Give Waybar's proxy time to notice the new name owner before signalling.
  g_usleep(2 * G_USEC_PER_SEC);

  for (int i = 1; i < argc; i++) {
    GVariant* params = g_variant_parse(NULL, argv[i], NULL, NULL, &err);
    if (params == NULL) {
      fprintf(stderr, "portal_peer: bad argument %s: %s\n", argv[i], err->message);
      return 1;
    }
    g_dbus_connection_emit_signal(bus, NULL, "/org/freedesktop/portal/desktop",
                                  "org.freedesktop.portal.Settings", "SettingChanged", params,
                                  NULL);
    g_dbus_connection_flush_sync(bus, NULL, NULL);
    g_usleep(G_USEC_PER_SEC / 2);
  }
  return 0;
}
