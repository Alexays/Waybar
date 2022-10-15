#pragma once

#include <fmt/format.h>

#include <csignal>
#include <string>

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include "bar.hpp"
#include "AModule.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

#include <unordered_map>

namespace waybar::modules {

class CustomList : public AModule {
 public:
  CustomList(const std::string&,  const waybar::Bar&, const std::string&,const Json::Value&);
  ~CustomList();
  auto update() -> void;
  void refresh(int /*signal*/);

 private:
  void delayWorker();
  void continuousWorker();
  void parseOutputRaw();
  void parseOutputJson();
  void handleEvent();
  bool handleScroll(GdkEventScroll* e);
  bool handleToggle(GdkEventButton* const& e);

  Gtk::Button &addButton(const Json::Value &node);
  void handleClick(std::string);

  std::unordered_map<std::string, Gtk::Button> buttons_;
  std::vector<Json::Value> results_;
  std::vector<Json::Value> prev_;
  const std::string name_;
  const Bar& bar_;
  Gtk::Box box_;
  std::chrono::seconds interval_;

  std::string id_;
  std::vector<std::string> class_;
  FILE* fp_;
  int pid_;
  util::command::res output_;
  util::JsonParser parser_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
