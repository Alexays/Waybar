#pragma once

#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>

#include <memory>
#include <string>
#include <utility>

#include "util/sleeper_thread.hpp"

namespace waybar::util {

/**
 * Keeps pthread cancellation off while the wrapped sink writes.
 *
 * Module workers are stopped with pthread_cancel(), and writing a message is a cancellation
 * point. spdlog calls each sink inside try { } catch (...). If the cancel fires there while the
 * worker is handling another exception, i.e. it logs an error from a catch block, catch (...)
 * meets abi::__forced_unwind with an exception already active and std::terminate aborts the
 * whole process. With cancellation off during the write, a pending cancel fires at the worker's
 * next cancellation point instead.
 */
class CancelSafeSink final : public spdlog::sinks::sink {
 public:
  explicit CancelSafeSink(spdlog::sink_ptr sink) : sink_(std::move(sink)) {
    set_level(sink_->level());
  }

  void log(const spdlog::details::log_msg& msg) override {
    CancellationGuard guard;
    sink_->log(msg);
  }
  void flush() override {
    CancellationGuard guard;
    sink_->flush();
  }
  void set_pattern(const std::string& pattern) override { sink_->set_pattern(pattern); }
  void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override {
    sink_->set_formatter(std::move(formatter));
  }

 private:
  spdlog::sink_ptr sink_;
};

// Wraps every sink of the logger in a CancelSafeSink. Call before worker threads start.
inline void makeLoggerCancelSafe(spdlog::logger& logger) {
  for (auto& sink : logger.sinks()) {
    if (dynamic_cast<CancelSafeSink*>(sink.get()) == nullptr) {
      sink = std::make_shared<CancelSafeSink>(sink);
    }
  }
}

}  // namespace waybar::util
