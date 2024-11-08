#pragma once

#include <fmt/format.h>
#include <sys/statvfs.h>

#include <fstream>
#include <gps.h>

#include "ALabel.hpp"
#include "util/format.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

  class Gps : public ALabel {
  public:
    Gps(const std::string&, const Json::Value&);
    virtual ~Gps();
    auto update() -> void override;

  private:
    const std::string getFixModeName() const;
    const std::string getFixModeString() const;

    const std::string getFixStatusString() const;

    util::SleeperThread thread_, gps_thread_;
    gps_data_t gps_data_;
    std::string state_;

    bool hideDisconnected = true;
    bool hideNoFix = false;
  };

}  // namespace waybar::modules
