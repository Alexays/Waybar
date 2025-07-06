#pragma once

#include <fmt/format.h>
#include <sys/statvfs.h>

#ifdef WANT_RFKILL
#include "util/rfkill.hpp"
#endif

#include <gps.h>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Gps : public ALabel {
 public:
  Gps(const std::string&, const Json::Value&);
  virtual ~Gps();
  auto update() -> void override;

 private:
#ifdef WANT_RFKILL
  util::Rfkill rfkill_;
#endif
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
