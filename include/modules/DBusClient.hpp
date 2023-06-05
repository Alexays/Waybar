#pragma once

#include "interfaces/IDBus.hpp"
#include "interfaces/IModule.hpp"
#include <json/json.h>
#include <spdlog/spdlog.h>

namespace waybar {

using resultMap = std::map<const std::string, Glib::ustring*>;

class DBusClient : public IModule, public IDBus {
 public:
  virtual ~DBusClient() = default;
  virtual auto resultCallback() -> void = 0;
  auto doAction(const Glib::ustring &name = "") -> void override;
 protected:
  DBusClient(const std::string &name);
  DBusClient(const DBusClient &) = delete;

  virtual resultMap* const getResultMap();

  Glib::ustring resLabel;
  Glib::ustring resTooltip;
  // Result map
  resultMap resultMap_ {
    {"getLabelText", &resLabel},
    {"getTooltipText", &resTooltip}
  };

 private:
  Glib::RefPtr<Gio::DBus::Proxy> dbusProxy_;
  Glib::VariantContainerBase dbusProxyResult_;
  std::vector<Glib::ustring> requests;
  resultMap *currentResultMap_;
  Glib::Variant<std::vector<std::tuple<Glib::ustring, Glib::ustring>>> varResult;
  auto proxyResult(Glib::RefPtr<Gio::AsyncResult> &result) -> void;
  auto proxyReady(Glib::RefPtr<Gio::AsyncResult> &result) -> void;
  };
}
