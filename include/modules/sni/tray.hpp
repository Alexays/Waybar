#pragma once

#include <fmt/format.h>
#include <thread>
#include "util/json.hpp"
#include "IModule.hpp"
#include "modules/sni/watcher.hpp"
#include "modules/sni/host.hpp"

namespace waybar::modules::SNI {

class Tray : public IModule {
  public:
    Tray(const Json::Value&);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    void onAdd(std::unique_ptr<Item>& item);
    void onRemove(std::unique_ptr<Item>& item);

    std::thread thread_;
    const Json::Value& config_;
    Gtk::Box box_;
    SNI::Watcher watcher_ ;
    SNI::Host host_;
};

}
