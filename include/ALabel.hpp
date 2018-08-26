#pragma once

#include <json/json.h>
#include "IModule.hpp"

namespace waybar {

class ALabel : public IModule {
  public:
    ALabel(const Json::Value&, const std::string format);
    virtual ~ALabel() = default;
    virtual auto update() -> void;
    virtual std::string getIcon(uint16_t percentage);
    virtual operator Gtk::Widget &();
  protected:
    Gtk::EventBox event_box_;
    Gtk::Label label_;
    const Json::Value& config_;
    std::string format_;
  private:
    bool handleToggle(GdkEventButton* const& ev);
    bool alt = false;
    const std::string default_format_;
};

}
