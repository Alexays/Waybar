#pragma once

#include <fmt/format.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"

namespace waybar::modules {

class IdleInhibitor : public ALabel {
  sigc::connection timeout_;
  sigc::connection activity_timeout_;
  sigc::connection motion_connection_;
  sigc::connection key_connection_;

 public:
  IdleInhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~IdleInhibitor();
  auto update() -> void override;
  auto refresh(int) -> void override;
  static std::list<waybar::AModule*> modules;
  static bool status;

 private:
  bool handleToggle(GdkEventButton* const& e) override;
  bool handleMotion(GdkEventMotion* const& e);
  bool handleKey(GdkEventKey* const& e);
  void toggleStatus();
  void resetActivityTimeout();
  void setupActivityMonitoring();
  void teardownActivityMonitoring();

  const Bar& bar_;
  struct zwp_idle_inhibitor_v1* idle_inhibitor_;
  int pid_;
  bool wait_for_activity_;
};

}  // namespace waybar::modules
