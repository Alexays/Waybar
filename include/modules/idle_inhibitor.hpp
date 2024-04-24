#pragma once

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"

namespace waybar::modules {

class IdleInhibitor final : public ALabel {
  sigc::connection timeout_;

 public:
  IdleInhibitor(const std::string&, const waybar::Bar&, const Json::Value&);
  virtual ~IdleInhibitor();
  auto update() -> void override;
  static std::list<waybar::AModule*> modules;
  static bool status;

 private:
  void handleToggle(int n_press, double dx, double dy);
  void toggleStatus();

  const Bar& bar_;
  struct zwp_idle_inhibitor_v1* idle_inhibitor_;
  int pid_;
};

}  // namespace waybar::modules
