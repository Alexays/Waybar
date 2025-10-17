#pragma once

#include <fmt/format.h>

#include <csignal>
#include <string>

#include "ALabel.hpp"
#include "modules/custom_exec_worker.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Custom : public ALabel, public CustomExecObserver {
 public:
  Custom(const std::string&, const std::string&, const Json::Value&, const std::string&);
  virtual ~Custom();
  auto update() -> void override;
  void refresh(int /*signal*/) override;

  // Implement CustomExecObserver
  void onExecOutput(const util::command::res& output) override;

 private:
  void delayWorker();
  void continuousWorker();
  void waitingWorker();
  void parseOutputRaw();
  void parseOutputJson();
  void handleEvent();
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
  util::command::res output_;
  util::JsonParser parser_;

  util::SleeperThread thread_;

  // Shared mode support
  bool shared_mode_;
  waybar::modules::WorkerKey worker_key_;

  waybar::modules::WorkerKey computeWorkerKey() const;
};

}  // namespace waybar::modules
