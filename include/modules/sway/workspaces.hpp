#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "util/json.hpp"
#include "IModule.hpp"
#include "modules/sway/ipc/client.hpp"

namespace waybar::modules::sway {

class Workspaces : public IModule {
  public:
    Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    void worker();
    void addWorkspace(Json::Value);
    std::string getIcon(std::string, Json::Value);
    bool handleScroll(GdkEventScroll*);
    std::string getPrevWorkspace();
    std::string getNextWorkspace();

    const Bar& bar_;
    const Json::Value& config_;
    waybar::util::SleeperThread thread_;
    Gtk::Box box_;
    util::JsonParser parser_;
    std::mutex mutex_;
    bool scrolling_;
    std::unordered_map<std::string, Gtk::Button> buttons_;
    Json::Value workspaces_;
    Ipc ipc_;
};

}
