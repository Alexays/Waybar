#pragma once

#include <fmt/format.h>
#include "ALabel.hpp"

namespace waybar::modules {

class Text : public ALabel {
  public:
    Text(const std::string&, const Json::Value&);
};

}
