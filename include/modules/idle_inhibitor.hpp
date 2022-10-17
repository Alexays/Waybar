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
  ~IdleInhibitor();
  auto update() -> void;
  static std::list<waybar::AModule*> modules;
  static bool status;

 private:
  bool handleToggle(GdkEventButton* const& e);
  void toggleStatus();

  const Bar& bar_;
  struct zwp_idle_inhibitor_v1* idle_inhibitor_;
  int pid_;
};

}  // namespace waybar::modules
