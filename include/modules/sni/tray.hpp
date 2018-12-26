#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "util/json.hpp"
#include "IModule.hpp"
#include "modules/sni/watcher.hpp"
#include "modules/sni/host.hpp"

namespace waybar::modules::SNI {

class Tray : public IModule {
  public:
    Tray(const std::string&, const Json::Value&);
    ~Tray() = default;
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    void onAdd(std::unique_ptr<Item>& item);
    void onRemove(std::unique_ptr<Item>& item);

    static inline std::size_t nb_hosts_ = 0;
    const Json::Value& config_;
    Gtk::Box box_;
    SNI::Watcher watcher_ ;
    SNI::Host host_;
};

}
