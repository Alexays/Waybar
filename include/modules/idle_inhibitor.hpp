#pragma once

#include <fmt/format.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"

namespace waybar::modules {

class IdleInhibitor : public ALabel {
  sigc::connection timeout_;
  struct ext_idle_notification_v1* idle_notification_;
  uint32_t idle_timeout_ms_;

 public:
  IdleInhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~IdleInhibitor();
  auto update() -> void override;
  auto refresh(int) -> void override;
  static std::list<waybar::AModule*> modules;
  static bool status;

 private:
  bool handleToggle(GdkEventButton* const& e) override;
  void toggleStatus();
  void setupIdleNotification();
  void teardownIdleNotification();
  static void handleIdled(void* data, struct ext_idle_notification_v1* notification);
  static void handleResumed(void* data, struct ext_idle_notification_v1* notification);

  const Bar& bar_;
  struct zwp_idle_inhibitor_v1* idle_inhibitor_;
  int pid_;
  bool wait_for_activity_;
};

}  // namespace waybar::modules
