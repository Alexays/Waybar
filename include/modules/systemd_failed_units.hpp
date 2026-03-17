#pragma once

#include <giomm/dbusproxy.h>

#include <string>

#include "ALabel.hpp"

namespace waybar::modules {

class SystemdFailedUnits : public ALabel {
 public:
  SystemdFailedUnits(const std::string&, const Json::Value&);
  virtual ~SystemdFailedUnits() = default;
  auto update() -> void override;

 private:
  bool hide_on_ok_;
  std::string format_ok_;

  bool update_pending_;
  std::string system_state_, user_state_, overall_state_;
  uint32_t nr_failed_system_, nr_failed_user_, nr_failed_;
  std::string last_status_;
  Glib::RefPtr<Gio::DBus::Proxy> system_props_proxy_, user_props_proxy_;

  void notify_cb(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                 const Glib::VariantContainerBase& arguments);
  void RequestFailedUnits();
  void RequestSystemState();
  void updateData();
};

}  // namespace waybar::modules
