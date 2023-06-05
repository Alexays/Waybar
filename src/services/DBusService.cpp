#include "services/DBusService.hpp"

namespace waybar::services {

DBusService::DBusService(const Json::Value& config, const std::string& name, const std::string& id,
                         const std::string& format)
    : Config(config, format),
      name_(std::move(name)),
      introspection_data_{Gio::DBus::NodeInfo::create_for_xml(
          Glib::ustring::sprintf(DBUS_INTROSPECTION_XML, name_))} {
  owner_id_ = Gio::DBus::own_name(
      Gio::DBus::BusType::BUS_TYPE_SESSION, Glib::ustring::sprintf(DBUS_NAME, name_),
      sigc::mem_fun(this, &DBusService::on_bus_acquired), sigc::ptr_fun(&on_name_acquired),
      sigc::mem_fun(this, &DBusService::on_name_lost),
      Gio::DBus::BusNameOwnerFlags::BUS_NAME_OWNER_FLAGS_NONE);
}

void DBusService::on_method_call(const Glib::RefPtr<Gio::DBus::Connection>& /* connection */,
                                 const Glib::ustring& /* sender */,
                                 const Glib::ustring& /* object_path */,
                                 const Glib::ustring& /* interface_name */,
                                 const Glib::ustring& method_name,
                                 const Glib::VariantContainerBase& parameters,
                                 const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation) {
  try {
    responseMap.clear();
    if (method_name == "doAction") {
      parameters.get_child(varValue, 0);
      if (!varValue.get().empty()) {
        for (const auto& rec : varValue.get()) {
          if (this->doActionExists(rec)) {
            this->doAction(rec);
            responseMap.push_back({method_name, rec});
          }

          if (Config::doActionExists(rec)) {
            Config::doAction(rec);
            responseMap.push_back({method_name, rec});
          }

          if (this->doMethodExists(rec)) responseMap.push_back({rec, this->doMethod(rec)});
        }

        varValue.get().clear();

        if (responseMap.empty()) {
          Gio::DBus::Error error(Gio::DBus::Error::UNKNOWN_METHOD, "Method does not exist.");
          throw error;
        }
      }
    } else {
      Gio::DBus::Error error(Gio::DBus::Error::UNKNOWN_METHOD, "Method does not exist.");
      throw error;
    }
    invocation->return_value(make_variant_tuple(responseMap));
  } catch (const Glib::Error& ex) {
    invocation->return_error(ex);
  }
};

void DBusService::on_bus_acquired(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                                  Glib::ustring) {
  try {
    registration_id_ =
        connection->register_object(Glib::ustring::sprintf(DBUS_OBJECT_PATH, name_),
                                    introspection_data_->lookup_interface(), interface_vtable_);
  } catch (const Glib::Error& ex) {
    if (ex.code() != 2)
      spdlog::error("Registration of object {0} failed. Error code: {1}. Error message: {2}",
                    Glib::ustring::sprintf(DBUS_OBJECT_PATH, name_).c_str(), ex.code(),
                    ex.what().c_str());
  }
}

void DBusService::on_name_acquired(const Glib::RefPtr<Gio::DBus::Connection>&, Glib::ustring) {}

void DBusService::on_name_lost(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                               Glib::ustring) {
  connection->unregister_object(registration_id_);
  this->stop();
}

auto DBusService::doAction(const Glib::ustring& name) -> void {
  this->doAction(name);
  Config::doAction(name);
}

const Glib::ustring& DBusService::getLabelText() { return labelText_; }

const Glib::ustring& DBusService::getTooltipText() { return tooltipText_; }

const Glib::ustring& DBusService::doMethod(const Glib::ustring& name) {
  if (doMethodExists(name))
    return (this->*methodMap_[name])();
  else {
    static const auto result{Glib::ustring::format("{0}. Unsupported method \"{1}\"", name_, name)};
    return result;
  }
}

const bool DBusService::doMethodExists(const Glib::ustring& name) {
  return (methodMap_.find(name) != methodMap_.cend());
}

}  // namespace waybar::services
