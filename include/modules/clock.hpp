#pragma once

#include <json/json.h>
#include <fmt/format.h>
#include "fmt/time.h"
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

class Clock : public IModule {
  public:
    Clock(Json::Value);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    Gtk::Label label_;
    waybar::util::SleeperThread thread_;
    Json::Value config_;
};

}
