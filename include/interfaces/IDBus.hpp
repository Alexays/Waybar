#ifndef DBUS_H
#define DBUS_H

#include <string>
#include <glibmm/ustring.h>
#include <giomm.h>
#include "util/sleeper_thread.hpp"

namespace waybar {

static constexpr auto DBUS_OBJECT_PATH{"/fr/arouillard/waybar/services/%s"};
static const Glib::ustring DBUS_NAME{"fr.arouillard.waybar.services.%s"};
static const Glib::ustring DBUS_INTROSPECTION_XML =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\""
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">"
    "<node>"
    "  <interface name='fr.arouillard.waybar.services.%s'>"
    "    <method name='doAction'>"
    "      <arg name='Name' direction='in' type='as'/>"
    "      <arg name='Result' direction='out' type='a(ss)'/>"
    "    </method>"
    "  </interface>"
    "</node>";

class IDBus {
 public:
  virtual ~IDBus() = default;
  template<typename... Ts>
  Glib::VariantContainerBase make_variant_tuple(Ts&&... args) {
      return Glib::VariantContainerBase::create_tuple(
        { Glib::Variant<std::remove_cv_t<std::remove_reference_t<Ts>>>::create(std::forward<Ts>(args))... });
  }
};

}

#endif
