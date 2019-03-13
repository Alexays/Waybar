#pragma once

#include <fmt/format.h>
#include <fstream>
#include "util/sleeper_thread.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Temperature : public ALabel {
  public:
    Temperature(const std::string&, const Json::Value&);
    ~Temperature() = default;
    auto update() -> void;
  private:
    std::tuple<uint16_t, uint16_t> getTemperature();
    bool isCritical(uint16_t);

    std::string file_path_;
    waybar::util::SleeperThread thread_;
};

}