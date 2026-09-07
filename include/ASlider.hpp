#pragma once

#include <chrono>

#include "AModule.hpp"
#include "gtkmm/scale.h"

namespace waybar {

class ASlider : public AModule {
 public:
  ASlider(const Json::Value& config, const std::string& name, const std::string& id,
          uint16_t interval = 0);
  virtual void onValueChanged();

 protected:
  bool vertical_ = false;
  int min_ = 0, max_ = 100, curr_ = 50;
  // Equivalent to AGraph/ALabel. 0 is a sentinel, see ALabel #4521.
  const std::chrono::milliseconds interval_;
  Gtk::Scale scale_;
};

}  // namespace waybar