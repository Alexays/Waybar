#pragma once

#include "AModule.hpp"
#include "gtkmm/scale.h"

namespace waybar {

class ASlider : public AModule {
 public:
  ASlider(const Json::Value& config, const std::string& name, const std::string& id);
  virtual void onValueChanged();

 protected:
  bool vertical_ = false;
  int min_ = 0, max_ = 100, curr_ = 50;
  Gtk::Scale scale_;
};

}  // namespace waybar