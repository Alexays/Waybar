#pragma once

#include <fmt/format.h>

#include <csignal>
#include <string>

#include "AModule.hpp"
#include "bar.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>

#include <unordered_map>

namespace waybar::modules {

class Custom : public AModule {
 public:
  Custom(const std::string&, const waybar::Bar&, const std::string&, const Json::Value&);
  ~Custom();
  auto update() -> void;
  void refresh(int /*signal*/);

 private:

  struct Node {
    std::string name_;
    std::string text_;
    std::string alt_;
    std::string tooltip_;
    std::string onclick_;
    bool hide_;
    std::vector<std::string> class_;
    int percentage_;
    Node() : name_("unnamed"),
             text_(""),
             alt_(""),
             tooltip_(""),
             onclick_(""),
             hide_(false),
             class_(std::vector<std::string>()),
             percentage_(0) {}
  };

  void delayWorker();
  void continuousWorker();
  void parseOutputRaw();
  void parseOutputJson();
  void handleEvent();
  bool handleScroll(GdkEventScroll* e);
  bool handleToggle(GdkEventButton* const& e);
  void handleClick(std::string name);
  Node parseItem(Json::Value &parsed);
  Gtk::Button &addButton(const Node &node);
  std::string getIcon(uint16_t percentage, const std::string& alt = "", uint16_t max = 0);

  const std::chrono::seconds interval_;
  util::command::res output_;

  std::vector<Node> results_;
  std::vector<Node> prev_;

  std::unordered_map<std::string, Gtk::Button> buttons_;
  Gtk::Box box_;
  std::string format_;

  FILE* fp_;
  int pid_;

  util::JsonParser parser_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
