#pragma once

#include <json/json.h>
#include <fmt/format.h>
#include "util/chrono.hpp"
#include "IModule.hpp"

namespace waybar::modules {

class Custom : public IModule {
  public:
    Custom(const std::string&, Json::Value);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    const std::string name_;
    Gtk::Label label_;
    waybar::util::SleeperThread thread_;
    Json::Value config_;
};

}
