#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/chrono.hpp"
#include "util/json.hpp"
#include "IModule.hpp"

namespace waybar::modules::sway {

class Workspaces : public IModule {
  public:
    Workspaces(waybar::Bar&, Json::Value);
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    void addWorkspace(Json::Value);
    std::string getIcon(std::string);
    bool handleScroll(GdkEventScroll*);
    int getPrevWorkspace();
    int getNextWorkspace();

    Bar& bar_;
    Json::Value config_;
    waybar::util::SleeperThread thread_;
    Gtk::Box box_;
    util::JsonParser parser_;
    std::mutex mutex_;
    bool scrolling_;
    std::unordered_map<int, Gtk::Button> buttons_;
    Json::Value workspaces_;
    int ipcfd_;
    int ipc_eventfd_;
};

}
