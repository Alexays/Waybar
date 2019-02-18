#pragma once

#include <fmt/format.h>
#include <iostream>
#include "util/sleeper_thread.hpp"
#include "util/command.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Light : public ALabel {
  public:
    Light(const std::string&, const Json::Value&);
    ~Light() = default;
    auto update() -> void;
  private:
    static inline const std::string cmd_get_ = "light -G";
    static inline const std::string cmd_increase_ = "light -A {}";
    static inline const std::string cmd_decrease_ = "light -U {}";

    void delayWorker();
    bool handleScroll(GdkEventScroll* e);
    void updateBrightness();

    uint8_t brightness_level_;
    bool scrolling_;
    waybar::util::SleeperThread thread_;
};

}  // namespace waybar::modules
