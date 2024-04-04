#pragma once

#include <fmt/format.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"

namespace waybar::modules {

class IdleInhibitor : public ALabel {
  sigc::connection timeout_;

 public:
  IdleInhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~IdleInhibitor();
  auto update() -> void override;
  static std::list<waybar::AModule*> modules;
  static bool status;
  static long deactivationTime;

 private:
  bool handleToggle(GdkEventButton* const& e) override;
  bool handleScroll(GdkEventScroll* e) override;

  void toggleStatus(int force_status = -1);

  const Bar& bar_;
  struct zwp_idle_inhibitor_v1* idle_inhibitor_;
  int pid_;

  bool dynamicTimeout;
  short timeout;
  short timeout_step;
};

}  // namespace waybar::modules
