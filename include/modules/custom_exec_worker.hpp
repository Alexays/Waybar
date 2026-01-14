#pragma once

#include <json/json.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "util/command.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

// Key identifying a unique worker based on exec configuration
struct WorkerKey {
  std::string exec;
  std::string exec_if;
  int64_t interval_ms;
  int64_t restart_interval_ms;
  int signal;

  bool operator==(const WorkerKey& other) const {
    return exec == other.exec && exec_if == other.exec_if && interval_ms == other.interval_ms &&
           restart_interval_ms == other.restart_interval_ms && signal == other.signal;
  }

  size_t hash() const {
    size_t h = std::hash<std::string>{}(exec);
    h ^= (std::hash<std::string>{}(exec_if) << 1);
    h ^= (std::hash<int64_t>{}(interval_ms) << 2);
    h ^= (std::hash<int64_t>{}(restart_interval_ms) << 3);
    h ^= (std::hash<int>{}(signal) << 4);
    return h;
  }
};

// Hash functor for WorkerKey
struct WorkerKeyHasher {
  size_t operator()(const WorkerKey& key) const { return key.hash(); }
};

// Observer interface for modules that want to receive exec output
class CustomExecObserver {
 public:
  virtual void onExecOutput(const util::command::res& output) = 0;
  virtual ~CustomExecObserver() = default;
};

// Singleton backend managing shared exec workers
class CustomExecWorker {
 private:
  CustomExecWorker() = default;  // Singleton

 public:
  ~CustomExecWorker();
  static CustomExecWorker& inst();

  // Subscribe to worker for given config
  void subscribe(const WorkerKey& key, const std::string& output_name,
                 CustomExecObserver* observer);
  void unsubscribe(const WorkerKey& key, CustomExecObserver* observer);

  // Wake worker (for signal-based workers)
  void wakeWorker(const WorkerKey& key);

 private:
  // State for a single shared worker
  struct WorkerState {
    WorkerKey key;
    std::string output_name;
    util::SleeperThread thread;
    util::command::res last_output;
    FILE* fp = nullptr;
    pid_t pid = -1;
    std::vector<CustomExecObserver*> observers;
    std::mutex mutex;
    std::vector<int> pid_children;

    void notifyObservers();
  };

  std::unordered_map<WorkerKey, std::unique_ptr<WorkerState>, WorkerKeyHasher> workers_;
  std::mutex workers_mutex_;

  void startWorker(const WorkerKey& key, const std::string& output_name);
  void stopWorker(const WorkerKey& key);

  // Worker thread functions
  void runDelayWorker(WorkerState* state);
  void runContinuousWorker(WorkerState* state);
  void runWaitingWorker(WorkerState* state);
};

}  // namespace waybar::modules
