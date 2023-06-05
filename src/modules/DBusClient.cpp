#include "modules/DBusClient.hpp"

namespace waybar {

DBusClient::DBusClient(const std::string &name) {
  Gio::DBus::Proxy::create_for_bus(
      Gio::DBus::BusType::BUS_TYPE_SESSION, Glib::ustring::sprintf(DBUS_NAME, name),
      Glib::ustring::sprintf(DBUS_OBJECT_PATH, name), Glib::ustring::sprintf(DBUS_NAME, name),
      sigc::mem_fun(this, &DBusClient::proxyReady));
}

resultMap *const DBusClient::getResultMap() { return &resultMap_; }

auto DBusClient::doAction(const Glib::ustring &name) -> void {
  if (dbusProxy_) {
    requests.clear();

    if (!name.empty()) requests.push_back(name);

    if (name.empty() || (resultMap_.find(name) != resultMap_.cend()))
      for (const auto &rec : *(this->getResultMap())) requests.push_back(rec.first);

    if (!requests.empty()) try {
        dbusProxy_->call("doAction", sigc::mem_fun(this, &DBusClient::proxyResult),
                         make_variant_tuple(requests));
      } catch (const Glib::Error &ex) {
        spdlog::error("Error code: {0}. Error message: {1}", ex.code(), ex.what().c_str());
      }
  }
}

auto DBusClient::proxyResult(Glib::RefPtr<Gio::AsyncResult> &_result) -> void {
  dbusProxyResult_ = dbusProxy_->call_finish(_result);
  currentResultMap_ = this->getResultMap();
  dbusProxyResult_.get_child(varResult);

  for (const auto &[methodName, value] : varResult.get())
    if (currentResultMap_->find(methodName) != currentResultMap_->cend())
      *(*currentResultMap_)[methodName] = value;

  varResult.get().clear();
  this->resultCallback();
}

auto DBusClient::proxyReady(Glib::RefPtr<Gio::AsyncResult> &_result) -> void {
  dbusProxy_ = Gio::DBus::Proxy::create_for_bus_finish(_result);
  doAction();
}

}  // namespace waybar
