#pragma once

#include <fmt/format.h>

#include <csignal>
#include <memory>
#include <string>

#include "ALabel.hpp"
#include "util/command.hpp"
#include "util/command_line_stream.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Custom : public ALabel {
 public:
  Custom(const std::string&, const std::string&, const Json::Value&, const std::string&);
  virtual ~Custom();
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void continuousWorker();
  void startContinuousProcess(bool throw_on_failure);
  void handleContinuousProcessExit(int exit_code);
  void scheduleContinuousRestart();
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
  std::string last_tooltip_markup_;
  const bool tooltip_format_enabled_;
  std::vector<std::string> class_;
  int percentage_;
  util::command::res output_;
  util::JsonParser parser_;
  std::unique_ptr<util::command::LineStream> continuous_stream_;
  sigc::connection restart_connection_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
