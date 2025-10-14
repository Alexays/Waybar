#pragma once

#include <fmt/format.h>
#include <sys/statvfs.h>

#include <fstream>

#include <libmm-glib/libmm-glib.h>

#include "ALabel.hpp"
#include "util/format.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

  class Wwan : public ALabel {
  public:
    Wwan(const std::string&, const Json::Value&);
    virtual ~Wwan();
    auto update() -> void override;

  private:

    void updateCurrentModem();

    util::SleeperThread thread_;
    std::string state_;
    GDBusConnection* connection;
    MMManager* manager;
    MMModem* current_modem;

    bool hideDisconnected = true;

    const std::string dbus_name = "org.freedesktop.ModemManager1";
    const std::string dbus_obj_path = "/org/freedesktop/ModemManager1/";
    const std::string dbus_modems_path = "/org/freedesktop/ModemManager1/Modems/";
  };

}  // namespace waybar::modules
