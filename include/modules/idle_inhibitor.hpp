#pragma once

#include <fmt/format.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"

struct ext_idle_notification_v1;

namespace waybar::modules {

class IdleInhibitor final : public ALabel {
  sigc::connection timeout_;
  ext_idle_notification_v1* idle_notification_;
  uint32_t idle_timeout_ms_;

 public:
  IdleInhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~IdleInhibitor();
  auto doUpdate() -> void override;

 private:
  auto doRefresh(int) -> void override;
  void handlePress(int n_press, double x, double y) override;
  bool handleScroll(double dx, double dy) override;

  void doToggle();
  void toggleStatus(int force_status = -1);
  void setupIdleNotification();
  void teardownIdleNotification();
  static void handleIdled(void* data, ext_idle_notification_v1* notification);
  static void handleResumed(void* data, ext_idle_notification_v1* notification);

  struct zwp_idle_inhibitor_v1* idle_inhibitor_;
  const Bar& bar_;
  int pid_;

  bool dynamicTimeout = false;
  double timeout;
  double timeout_step;
  bool wait_for_activity_;

  static std::list<waybar::AModule*> modules;
  static bool status;
  static long deactivationTime;
};

}  // namespace waybar::modules
