#pragma once

#include <fmt/format.h>

#include "ALabel.hpp"
#include "bar.hpp"
#include "client.hpp"

namespace wabar::modules {

class IdleInhibitor : public ALabel {
  sigc::connection timeout_;

 public:
  IdleInhibitor(const std::string&, const wabar::Bar&, const Json::Value&);
  virtual ~IdleInhibitor();
  auto update() -> void override;
  static std::list<wabar::AModule*> modules;
  static bool status;

 private:
  bool handleToggle(GdkEventButton* const& e) override;
  void toggleStatus();

  const Bar& bar_;
  struct zwp_idle_inhibitor_v1* idle_inhibitor_;
  int pid_;
};

}  // namespace wabar::modules
