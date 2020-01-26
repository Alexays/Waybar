#pragma once

#include <linux/rfkill.h>

namespace waybar::util {

class Rfkill {
 public:;
  Rfkill(enum rfkill_type rfkill_type);
  ~Rfkill() = default;
  void waitForEvent();
  int getState() const;

 private:
  enum rfkill_type rfkill_type_;
  int state_ = 0;
  int prev_state_ = 0;
};

}  // namespace waybar::util
