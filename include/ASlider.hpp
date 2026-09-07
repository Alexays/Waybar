#pragma once

#include <chrono>

#include "AModule.hpp"
#include "gtkmm/scale.h"

namespace waybar {

class ASlider : public AModule {
 public:
  ASlider(const Json::Value& config, const std::string& name, const std::string& id,
          uint16_t interval = 0);

 protected:
  // Only write hook; user-initiated changes only. See handleChangeValue.
  virtual void onCommit(int value) = 0;
  // Programmatic display update; does not emit onCommit.
  void setValueSilently(int value);

  bool vertical_ = false;
  int min_ = 0, max_ = 100, curr_ = 50;
  // Equivalent to AGraph/ALabel. 0 is a sentinel, see ALabel #4521.
  const std::chrono::milliseconds interval_;
  // Guards against a stray write while we drive the scale ourselves.
  bool updating_ = false;
  Gtk::Scale scale_;

 private:
  bool handleChangeValue(Gtk::ScrollType scroll_type, double new_value);
};

}  // namespace waybar