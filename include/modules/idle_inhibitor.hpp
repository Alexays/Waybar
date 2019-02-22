#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "ALabel.hpp"

namespace waybar::modules {

class IdleInhibitor: public ALabel {
  public:
    IdleInhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
    ~IdleInhibitor();
    auto update() -> void;
  private:
    bool onClick(GdkEventButton* const& ev);

    const Bar& bar_;
    std::string status_;
    struct zwp_idle_inhibitor_v1 *idle_inhibitor_;
};

}
