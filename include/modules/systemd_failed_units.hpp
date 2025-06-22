#pragma once

#include <giomm/dbusproxy.h>

#include <string>

#include "ALabel.hpp"

namespace waybar::modules {

class SystemdFailedUnits : public ALabel {
 public:
  SystemdFailedUnits(const std::string &, const Json::Value &);
  virtual ~SystemdFailedUnits();
  auto update() -> void override;

 private:
  bool hide_on_ok;
  std::string format_ok;

  bool update_pending;
  std::string system_state, user_state, overall_state;
  uint32_t nr_failed_system, nr_failed_user, nr_failed;
  std::string last_status;
  Glib::RefPtr<Gio::DBus::Proxy> system_proxy, user_proxy;

  void notify_cb(const Glib::ustring &sender_name, const Glib::ustring &signal_name,
                 const Glib::VariantContainerBase &arguments);
  void RequestFailedUnits();
  void RequestSystemState();
  void updateData();
};

}  // namespace waybar::modules
