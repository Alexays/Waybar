#pragma once

#include <chrono>
#include <csignal>
#include <optional>
#include <string>

#include "util/command.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::util {

/**
 * Runs a child process and reads output from it, providing it to
 * the callbacks.
 */
class WorkerThread {
 public:
  WorkerThread() : pid_(-1) {}

  WorkerThread(const Json::Value& config, std::function<void(std::string)> output_callback,
               std::function<void(int)> exit_callback)
      : output_callback_(output_callback), exit_callback_(exit_callback), pid_(-1) {
    if (config["exec"].isString()) {
      exec_ = config["exec"].asString();
    }
    if (config["exec_if"].isString()) {
      exec_if_ = config["exec_if"].asString();
    }
    if (config["interval"].isUInt()) {
      interval_ = std::chrono::seconds(config["interval"].asUInt());
    } else if (config["interval"] == "once") {
      interval_ = std::chrono::seconds(100000000);
    } else {
      interval_ = std::chrono::seconds(0);
    }
    if (config["restart-interval"].isUInt()) {
      restart_interval_ = std::chrono::seconds(config["restart-interval"].asUInt());
    }
    if (config["signal"].isInt()) {
      signal_ = SIGRTMIN + config["signal"].asInt();
    }

    if (interval_.count() > 0) {
      thread_ = [this] { delay_worker(); };
    } else if (!exec_.empty()) {
      thread_ = [this] { continuous_worker(); };
    }
  }

  ~WorkerThread() {
    if (pid_ != -1) {
      killpg(pid_, SIGTERM);
      pid_ = -1;
    }
  }

  void refresh(int signal) {
    if (signal_.has_value() && *signal_ == signal) {
      wake_up();
    }
  }

  void wake_up() { thread_.wake_up(); }

 private:
  void delay_worker() {
    bool can_update = true;
    if (!exec_if_.empty()) {
      util::command::res output = util::command::execNoRead(exec_if_);
      if (output.exit_code != 0) {
        can_update = false;
        exit_callback_(output.exit_code);
      }
    }
    if (can_update) {
      if (!exec_.empty()) {
        util::command::res output = util::command::exec(exec_);
        if (output.exit_code == 0) {
          output_callback_(std::move(output.out));
        } else {
          exit_callback_(output.exit_code);
        }
      }
    }
    thread_.sleep_for(interval_);
  }

  void continuous_worker() {
    pid_ = -1;
    FILE* fp = util::command::open(exec_, pid_);
    if (!fp) {
      throw std::runtime_error("Unable to open " + exec_);
    }
    while (true) {
      char*  buff = nullptr;
      size_t len = 0;
      if (getline(&buff, &len, fp) == -1) {
        int exit_code = 1;
        if (fp) {
          exit_code = WEXITSTATUS(util::command::close(fp, pid_));
          fp = nullptr;
        }
        if (exit_code != 0) {
          spdlog::error("'{}' stopped unexpectedly, is it endless?", exec_);
          exit_callback_(exit_code);
        }
        if (restart_interval_.has_value()) {
          pid_ = -1;
          thread_.sleep_for(std::chrono::seconds(*restart_interval_));
          fp = util::command::open(exec_, pid_);
          if (!fp) {
            throw std::runtime_error("Unable to open " + exec_);
          }
        } else {
          thread_.stop();
          return;
        }
      } else {
        std::string output = buff;

        // Remove last newline
        if (!output.empty() && output[output.length() - 1] == '\n') {
          output.erase(output.length() - 1);
        }

        output_callback_(std::move(output));
      }
    }
  }

  std::string                         exec_;
  std::string                         exec_if_;
  std::chrono::seconds                interval_;
  std::optional<std::chrono::seconds> restart_interval_;
  std::optional<int>                  signal_;
  std::function<void(std::string)>    output_callback_;
  std::function<void(int)>            exit_callback_;
  int                                 pid_;
  SleeperThread                       thread_;
};

}  // namespace waybar::util
