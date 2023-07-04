#pragma once

#include <sndio.h>

#include <vector>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Sndio : public ALabel {
 public:
  Sndio(const std::string &, const Json::Value &);
  virtual ~Sndio();
  auto update() -> void override;
  auto set_desc(struct sioctl_desc *, unsigned int) -> void;
  auto put_val(unsigned int, unsigned int) -> void;
  bool handleScroll(GdkEventScroll *) override;
  bool handleToggle(GdkEventButton *const &) override;

 private:
  auto connect_to_sndio() -> void;
  util::SleeperThread thread_;
  struct sioctl_hdl *hdl_;
  std::vector<struct pollfd> pfds_;
  unsigned int addr_;
  unsigned int volume_, old_volume_, maxval_;
  bool muted_;
};

}  // namespace waybar::modules
