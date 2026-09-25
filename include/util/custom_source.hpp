#pragma once

#include <json/json.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "util/command.hpp"
#include "util/command_line_stream.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::util {

/**
 * The read half of a custom/ module: runs an exec / exec-if / signal / continuous
 * command per config and exposes the parsed output. Copied from modules::Custom,
 * with two slider-safe differences from that family's parsers: a parse failure
 * yields ok=false (never a synthesised 0), and no value is clamped to [0,100] (the
 * consumer clamps to its own range). Reaping of action-command children stays with
 * the owning AModule, so the workers here omit it.
 */
class CustomSource {
 public:
  struct Output {
    std::string text;
    std::string tooltip;
    std::vector<std::string> classes;
    std::optional<int> percentage;  // json return-type only; nullopt when absent
    bool ok = false;                // false on command failure or empty output
  };

  // on_update is invoked from a worker thread when new output is ready; the
  // consumer marshals to the GTK thread (e.g. dp.emit()) and calls parse() there.
  CustomSource(const Json::Value& config, std::string output_name, std::function<void()> on_update);
  ~CustomSource();

  void refresh(int signal);
  // Parse the most recent read output.
  Output parse() const;
  // Parse an explicit result; parse() applies this to the last read. Exposed so
  // unit tests can drive the parsers without spawning a worker.
  Output parse(const command::res& output) const;

 private:
  void delayWorker();
  void continuousWorker();
  void startContinuousProcess(bool throw_on_failure);
  void handleContinuousProcessExit(int exit_code);
  void scheduleContinuousRestart();
  void waitingWorker();
  Output parseRaw(const command::res& output) const;
  Output parseJson(const command::res& output) const;

  Json::Value config_;
  std::string output_name_;
  std::function<void()> on_update_;
  std::chrono::milliseconds interval_;
  command::res output_{.exit_code = 0, .out = ""};
  std::unique_ptr<command::LineStream> continuous_stream_;
  sigc::connection restart_connection_;
  SleeperThread thread_;
};

}  // namespace waybar::util
