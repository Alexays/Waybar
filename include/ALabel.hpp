#pragma once

#include <json/json.h>
#include "IModule.hpp"

namespace waybar {

class ALabel : public IModule {
  public:
    ALabel(Json::Value);
    virtual ~ALabel() = default;
    virtual auto update() -> void;
    virtual operator Gtk::Widget &();
  protected:
    Gtk::Label label_;
    Json::Value config_;
};

}
