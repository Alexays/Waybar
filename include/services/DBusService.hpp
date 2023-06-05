#pragma once

#include "interfaces/IModule.hpp"
#include "interfaces/IDBus.hpp"
#include "modules/config.hpp"

namespace waybar::services {

class DBusService : public IModule, public IDBus, public modules::Config {
 public:
  virtual ~DBusService() = default;

  void on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection>& connection, Glib::ustring);
  static void on_name_acquired(const Glib::RefPtr<Gio::DBus::Connection>&, Glib::ustring);
  void on_name_lost(const Glib::RefPtr<Gio::DBus::Connection>&, Glib::ustring);
  void on_method_call(const Glib::RefPtr<Gio::DBus::Connection>& /* connection */,
                      const Glib::ustring& /* sender */,
                      const Glib::ustring& /* object_path */,
                      const Glib::ustring& /* interface_name */,
                      const Glib::ustring& method_name,
                      const Glib::VariantContainerBase& parameters,
                      const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);

  const Gio::DBus::InterfaceVTable interface_vtable_{sigc::mem_fun(this, &DBusService::on_method_call)};
  auto doAction(const Glib::ustring &name) -> void override;
  virtual bool doActionExists(const Glib::ustring &name) = 0;
  virtual const Glib::ustring& doMethod(const Glib::ustring &name);
  virtual const bool doMethodExists(const Glib::ustring &name);

  virtual auto start() -> void = 0;
  virtual auto stop() -> void = 0;
 protected:
  DBusService(const Json::Value &, const std::string &, const std::string &,
              const std::string &);
  DBusService(const DBusService &) = delete;

  virtual const Glib::ustring& getLabelText();
  virtual const Glib::ustring& getTooltipText();
  Glib::ustring labelText_;
  Glib::ustring tooltipText_;
 private:
  const std::string name_;
  const Glib::RefPtr<Gio::DBus::NodeInfo> introspection_data_;
  guint owner_id_;
  guint registration_id_{0};
  std::vector<std::tuple<Glib::ustring, Glib::ustring>> responseMap;
  Glib::Variant<std::vector<Glib::ustring>> varValue;

  // Action map
  static inline std::map<const std::string, const Glib::ustring&(waybar::services::DBusService::*const)()> methodMap_ {
    {"getLabelText", &waybar::services::DBusService::getLabelText},
    {"getTooltipText", &waybar::services::DBusService::getTooltipText}
  };
};
}
