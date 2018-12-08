#pragma once

#include <fmt/format.h>
#include <iostream>
#include "util/chrono.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Custom : public ALabel {
  public:
    Custom(const std::string, const Json::Value&);
    ~Custom();
    auto update() -> void;
  private:
    void delayWorker();
    void continuousWorker();
    void parseOutputRaw();
    void parseOutputJson();

    const std::string name_;
    std::string text_;
    std::string tooltip_;
    std::string class_;
    std::string prevclass_;
    waybar::util::SleeperThread thread_;
    waybar::util::command::res output_;
    waybar::util::JsonParser parser_;
    FILE* fp_;
};

}
