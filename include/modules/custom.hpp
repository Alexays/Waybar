#pragma once

#include <fmt/format.h>
#include <iostream>
#include "util/chrono.hpp"
#include "util/command.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Custom : public ALabel {
  public:
    Custom(const std::string, const Json::Value&);
    auto update() -> void;
  private:
    void delayWorker();
    void continuousWorker();
    void parseOutput();

    const std::string name_;
    std::string text_;
    std::string tooltip_;
    std::string class_;
    std::string prevclass_;
    waybar::util::SleeperThread thread_;
    waybar::util::command::res output_;
};

}
