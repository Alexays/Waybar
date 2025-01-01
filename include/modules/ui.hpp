#pragma once

#include "AModule.hpp"

namespace waybar::modules {

class UI final : public AModule {
 public:
  UI(const std::string&, const std::string&, const Json::Value&);
  virtual ~UI() = default;
  operator Gtk::Widget&() override;

 private:
  Glib::RefPtr<Gtk::Widget> uiWg_;
};

}  // namespace waybar::modules
