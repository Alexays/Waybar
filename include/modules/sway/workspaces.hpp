#pragma once

#include <fmt/format.h>
#include "bar.hpp"
#include "client.hpp"
#include "util/sleeper_thread.hpp"
#include "util/json.hpp"
#include "IModule.hpp"
#include "modules/sway/ipc/client.hpp"
#include <gtkmm/button.h>

namespace waybar::modules::sway {

class Workspaces : public IModule {
  public:
    Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
    ~Workspaces() = default;
    auto update() -> void;
    operator Gtk::Widget &();
  private:
    void worker();
    void addWorkspace(const Json::Value&);
		void onButtonReady(const Json::Value&, Gtk::Button&);
    std::string getIcon(const std::string&, const Json::Value&);
    bool handleScroll(GdkEventScroll*);
    std::string getPrevWorkspace();
    std::string getNextWorkspace();
    uint16_t getWorkspaceIndex(const std::string &name);
    std::string trimWorkspaceName(std::string);

    const Bar& bar_;
    const Json::Value& config_;
    Json::Value workspaces_;
    waybar::util::SleeperThread thread_;
    Gtk::Box box_;
    util::JsonParser parser_;
    Ipc ipc_;
    std::mutex mutex_;
    bool scrolling_;
    std::unordered_map<std::string, Gtk::Button> buttons_;
};

}
