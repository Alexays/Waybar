#include "modules/custom_exec_worker.hpp"

#include <spdlog/spdlog.h>

#include "util/scope_guard.hpp"

namespace waybar::modules {

CustomExecWorker::~CustomExecWorker() {
  std::lock_guard<std::mutex> lock(workers_mutex_);
  for (auto& [key, state] : workers_) {
    stopWorker(key);
  }
  workers_.clear();
}

CustomExecWorker& CustomExecWorker::inst() {
  static CustomExecWorker instance;
  return instance;
}

void CustomExecWorker::subscribe(const WorkerKey& key, const std::string& output_name,
                                  CustomExecObserver* observer) {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  auto it = workers_.find(key);
  if (it == workers_.end()) {
    // First subscriber - create new worker
    auto state = std::make_unique<WorkerState>();
    state->key = key;
    state->output_name = output_name;

    workers_[key] = std::move(state);
    it = workers_.find(key);
    startWorker(key, output_name);
  }

  // Add observer to worker
  {
    std::lock_guard<std::mutex> state_lock(it->second->mutex);
    it->second->observers.push_back(observer);

    // Immediately send last output if available
    if (!it->second->last_output.out.empty() || it->second->last_output.exit_code != 0) {
      observer->onExecOutput(it->second->last_output);
    }
  }
}

void CustomExecWorker::unsubscribe(const WorkerKey& key, CustomExecObserver* observer) {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  auto it = workers_.find(key);
  if (it == workers_.end()) return;

  bool should_cleanup = false;
  {
    std::lock_guard<std::mutex> state_lock(it->second->mutex);
    auto& observers = it->second->observers;
    observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());

    // Check if we should cleanup
    should_cleanup = observers.empty();
  }

  // Last subscriber - clean up worker
  if (should_cleanup) {
    stopWorker(key);
    workers_.erase(it);
  }
}

void CustomExecWorker::wakeWorker(const WorkerKey& key) {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  auto it = workers_.find(key);
  if (it != workers_.end()) {
    it->second->thread.wake_up();
  }
}

void CustomExecWorker::startWorker(const WorkerKey& key, const std::string& output_name) {
  auto it = workers_.find(key);
  if (it == workers_.end()) return;

  WorkerState* state = it->second.get();

  // Determine worker type based on key
  bool has_signal = key.signal != 0;
  bool has_interval = key.interval_ms > 0;
  bool has_exec = !key.exec.empty();

  if (has_signal && !has_interval && key.restart_interval_ms == 0) {
    // Signal-based worker
    runWaitingWorker(state);
  } else if (has_interval) {
    // Interval-based worker
    runDelayWorker(state);
  } else if (has_exec) {
    // Continuous worker
    runContinuousWorker(state);
  }
}

void CustomExecWorker::stopWorker(const WorkerKey& key) {
  auto it = workers_.find(key);
  if (it == workers_.end()) return;

  WorkerState* state = it->second.get();

  // Stop thread
  state->thread.stop();

  // Kill process if running
  if (state->pid != -1) {
    killpg(state->pid, SIGTERM);
    waitpid(state->pid, NULL, 0);
    state->pid = -1;
  }

  // Close file pointer
  if (state->fp != nullptr) {
    fclose(state->fp);
    state->fp = nullptr;
  }
}

void CustomExecWorker::WorkerState::notifyObservers() {
  std::lock_guard<std::mutex> lock(mutex);
  for (auto* observer : observers) {
    observer->onExecOutput(last_output);
  }
}

void CustomExecWorker::runDelayWorker(WorkerState* state) {
  state->thread = [this, state] {
    // Wait for child processes
    for (int i : state->pid_children) {
      int status;
      waitpid(i, &status, 0);
    }
    state->pid_children.clear();

    bool can_update = true;
    if (!state->key.exec_if.empty()) {
      state->last_output = util::command::execNoRead(state->key.exec_if);
      if (state->last_output.exit_code != 0) {
        can_update = false;
        state->notifyObservers();
      }
    }

    if (can_update && !state->key.exec.empty()) {
      state->last_output = util::command::exec(state->key.exec, state->output_name);
      state->notifyObservers();
    }

    state->thread.sleep_for(std::chrono::milliseconds(state->key.interval_ms));
  };
}

void CustomExecWorker::runContinuousWorker(WorkerState* state) {
  auto cmd = state->key.exec;
  state->pid = -1;
  state->fp = util::command::open(cmd, state->pid, state->output_name);
  if (!state->fp) {
    spdlog::error("CustomExecWorker: Unable to open command: {}", cmd);
    return;
  }

  state->thread = [this, state, cmd] {
    char* buff = nullptr;
    waybar::util::ScopeGuard buff_deleter([&buff]() {
      if (buff) {
        free(buff);
      }
    });
    size_t len = 0;

    if (getline(&buff, &len, state->fp) == -1) {
      int exit_code = 1;
      if (state->fp) {
        exit_code = WEXITSTATUS(util::command::close(state->fp, state->pid));
        state->fp = nullptr;
      }

      if (exit_code != 0) {
        state->last_output = {exit_code, ""};
        state->notifyObservers();
        spdlog::error("CustomExecWorker: Command stopped unexpectedly, is it endless? {}", cmd);
      }

      if (state->key.restart_interval_ms > 0) {
        state->pid = -1;
        state->thread.sleep_for(std::chrono::milliseconds(state->key.restart_interval_ms));
        state->fp = util::command::open(cmd, state->pid, state->output_name);
        if (!state->fp) {
          spdlog::error("CustomExecWorker: Unable to reopen command: {}", cmd);
          state->thread.stop();
          return;
        }
      } else {
        state->thread.stop();
        return;
      }
    } else {
      std::string output = buff;

      // Remove last newline
      if (!output.empty() && output[output.length() - 1] == '\n') {
        output.erase(output.length() - 1);
      }

      state->last_output = {0, output};
      state->notifyObservers();
    }
  };
}

void CustomExecWorker::runWaitingWorker(WorkerState* state) {
  state->thread = [this, state] {
    bool can_update = true;
    if (!state->key.exec_if.empty()) {
      state->last_output = util::command::execNoRead(state->key.exec_if);
      if (state->last_output.exit_code != 0) {
        can_update = false;
        state->notifyObservers();
      }
    }

    if (can_update && !state->key.exec.empty()) {
      state->last_output = util::command::exec(state->key.exec, state->output_name);
      state->notifyObservers();
    }

    state->thread.sleep();
  };
}

}  // namespace waybar::modules
