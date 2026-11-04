#pragma once

#include <fmt/format.h>

#include <csignal>
#include <functional>
#include <mutex>
#include <string>

#include "AGraph.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/scope_guard.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class CustomGraph : public AGraph {
 public:
  CustomGraph(const std::string&, const std::string&, const Json::Value&, const std::string&);
  virtual ~CustomGraph();
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void continuousWorker();
  void waitingWorker();
  void parseOutputRaw();
  void parseOutputJson();
  void handleEvent();
  void setOutput(util::command::res output);
  bool handleScroll(GdkEventScroll* e) override;
  bool handleToggle(GdkEventButton* const& e) override;

  const std::string name_;
  const std::string output_name_;
  std::string text_;
  std::string id_;
  std::string alt_;
  std::string tooltip_;
  const bool tooltip_format_enabled_;
  std::vector<std::string> class_;
  int percentage_;
  FILE* fp_;
  int pid_;
  // Written by the worker thread, read by update() on the main thread
  std::mutex output_mutex_;
  util::command::res output_;
  util::JsonParser parser_;
  // Declared before thread_, so that it runs once the worker is joined
  util::ScopeGuard<std::function<void()>> release_command_{
      [this] { util::command::abandon(fp_, pid_); }};

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
