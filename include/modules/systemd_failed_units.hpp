#pragma once

#include <giomm/dbusproxy.h>

#include <string>
#include <vector>

#include "ALabel.hpp"

namespace waybar::modules {

class SystemdFailedUnits : public ALabel {
 public:
  SystemdFailedUnits(const std::string&, const Json::Value&, std::mutex&, std::list<pid_t>&);
  virtual ~SystemdFailedUnits() = default;
  auto update() -> void override;

 private:
  struct FailedUnit {
    std::string name;
    std::string description;
    std::string load_state;
    std::string active_state;
    std::string sub_state;
    std::string scope;
  };

  bool hide_on_ok_;
  std::string format_ok_;
  std::string tooltip_format_;
  std::string tooltip_format_ok_;
  std::string tooltip_unit_format_;

  bool update_pending_;
  std::string system_state_, user_state_, overall_state_;
  uint32_t nr_failed_system_, nr_failed_user_, nr_failed_;
  std::string last_status_;
  Glib::RefPtr<Gio::DBus::Proxy> system_props_proxy_, user_props_proxy_;
  Glib::RefPtr<Gio::DBus::Proxy> system_manager_proxy_, user_manager_proxy_;
  std::vector<FailedUnit> failed_units_;

  void notify_cb(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                 const Glib::VariantContainerBase& arguments);
  void RequestFailedUnits();
  void RequestFailedUnitsList();
  void RequestSystemState();
  std::vector<FailedUnit> LoadFailedUnitsList(const char* kind,
                                              Glib::RefPtr<Gio::DBus::Proxy>& proxy,
                                              const std::string& scope);
  std::string BuildTooltipFailedList() const;
  void updateData();
};

}  // namespace waybar::modules
