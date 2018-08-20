#pragma once

#include <json/json.h>
#include "IModule.hpp"

namespace waybar {

class ALabel : public IModule {
  public:
    ALabel(const Json::Value&);
    virtual ~ALabel() = default;
    virtual auto update() -> void;
    virtual operator Gtk::Widget &();
  protected:
    Gtk::Label label_;
    const Json::Value& config_;
};

}
