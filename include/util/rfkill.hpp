#pragma once

#include <glibmm/iochannel.h>
#include <linux/rfkill.h>
#include <sigc++/signal.h>
#include <sigc++/trackable.h>

namespace waybar::util {

class Rfkill : public sigc::trackable {
 public:
  Rfkill(enum rfkill_type rfkill_type);
  ~Rfkill();
  bool getState() const;

  sigc::signal<void(struct rfkill_event&)> on_update;

 private:
  enum rfkill_type rfkill_type_;
  bool state_ = false;
  int fd_ = -1;

  bool on_event(Glib::IOCondition cond);
};

}  // namespace waybar::util
