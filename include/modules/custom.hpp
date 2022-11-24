#pragma once

#include <fmt/format.h>

#include <csignal>
#include <string>

#include "ALabel.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Custom : public ALabel {
 public:
  Custom(const std::string&, const std::string&, const Json::Value&);
  ~Custom();
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

  const std::string name_;
  std::string text_;
  std::string id_;
  std::string alt_;
  std::string tooltip_;
  std::vector<std::string> class_;
  int percentage_;
  FILE* fp_;
  int pid_;
  util::command::res output_;
  util::JsonParser parser_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
