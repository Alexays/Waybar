#pragma once

#include <fmt/format.h>
#include <iostream>
#include <csignal>
#include "util/sleeper_thread.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class Custom : public ALabel {
  public:
    Custom(const std::string&, const Json::Value&);
    ~Custom();
    auto update() -> void;
    void refresh(int /*signal*/);
  private:
    void delayWorker();
    void continuousWorker();
    void parseOutputRaw();
    void parseOutputJson();

    const std::string name_;
    std::string text_;
    std::string alt_;
    std::string tooltip_;
    std::vector<std::string> class_;
    int percentage_;
    waybar::util::SleeperThread thread_;
    waybar::util::command::res output_;
    waybar::util::JsonParser parser_;
    FILE* fp_;
};

}
