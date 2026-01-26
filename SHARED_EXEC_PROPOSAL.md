# Shared Exec Worker Proposal for Custom Modules

## Problem Statement

Currently, each `custom` module instance runs its own exec command worker. When multiple bars are displayed on different outputs (monitors), identical custom module definitions result in duplicate command execution. For example, if 3 monitors each have a bar with `custom/weather`, the weather script runs 3 times with identical output.

This wastes CPU resources, makes unnecessary API calls, and can cause issues with rate-limited services.

## Proposed Solution

Implement a **shared worker backend** similar to the existing compositor backends (Hyprland IPC, Niri backend, etc.) that allows multiple module instances to subscribe to a single exec worker's output.

### Key Design Principles

1. **Opt-in via config**: Backward compatible - requires `"shared": true` flag
2. **Singleton pattern**: Similar to `hyprland::IPC::inst()` pattern used in the codebase
3. **Key-based identification**: Workers identified by hash of (exec command, interval, exec-if, restart-interval)
4. **Observer pattern**: Multiple Custom module instances register as observers to shared worker
5. **Thread-safe**: Use mutex protection for subscriber list and output state

## Architecture

### New Components

#### 1. `CustomExecWorker` - Shared Worker Backend
**Location**: `include/modules/custom_exec_worker.hpp` and `src/modules/custom_exec_worker.cpp`

```cpp
namespace waybar::modules {

struct WorkerKey {
  std::string exec;
  std::string exec_if;
  int64_t interval_ms;
  int64_t restart_interval_ms;

  bool operator==(const WorkerKey& other) const;
  size_t hash() const;
};

class CustomExecObserver {
 public:
  virtual void onExecOutput(const util::command::res& output) = 0;
  virtual ~CustomExecObserver() = default;
};

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

  // For signal-based workers
  void wakeWorker(const WorkerKey& key);

 private:
  struct WorkerState {
    WorkerKey key;
    std::string output_name;
    util::SleeperThread thread;
    util::command::res last_output;
    FILE* fp = nullptr;
    pid_t pid = -1;
    std::vector<CustomExecObserver*> observers;
    std::mutex mutex;

    void notifyObservers();
  };

  std::unordered_map<WorkerKey, std::unique_ptr<WorkerState>,
                     WorkerKeyHasher> workers_;
  std::mutex workers_mutex_;

  void startWorker(const WorkerKey& key, const std::string& output_name);
  void stopWorker(const WorkerKey& key);

  // Worker thread functions (similar to Custom module but notify observers)
  void runDelayWorker(WorkerState* state);
  void runContinuousWorker(WorkerState* state);
  void runWaitingWorker(WorkerState* state);
};

}  // namespace waybar::modules
```

#### 2. Modified `Custom` Module
**Location**: `include/modules/custom.hpp` and `src/modules/custom.cpp`

Add to `Custom` class:
```cpp
class Custom : public ALabel, public CustomExecObserver {
 public:
  // Existing constructor
  Custom(const std::string&, const std::string&, const Json::Value&, const std::string&);
  ~Custom() override;

  // Implement CustomExecObserver
  void onExecOutput(const util::command::res& output) override;

 private:
  // Existing members...

  // New members for shared mode
  bool shared_mode_;
  WorkerKey worker_key_;

  // Methods to compute worker key from config
  WorkerKey computeWorkerKey() const;
  bool isSharedMode() const;
};
```

### Implementation Details

#### Worker Key Generation

The worker key uniquely identifies a worker based on:
```cpp
WorkerKey CustomExecWorker::computeWorkerKey(const Json::Value& config) {
  WorkerKey key;
  key.exec = config["exec"].asString();
  key.exec_if = config["exec-if"].asString();
  key.interval_ms = config["interval"].isNumeric()
      ? static_cast<int64_t>(config["interval"].asDouble() * 1000) : 0;
  key.restart_interval_ms = config["restart-interval"].isNumeric()
      ? static_cast<int64_t>(config["restart-interval"].asDouble() * 1000) : 0;
  return key;
}

size_t WorkerKey::hash() const {
  // Combine hashes of all fields
  size_t h = std::hash<std::string>{}(exec);
  h ^= std::hash<std::string>{}(exec_if) << 1;
  h ^= std::hash<int64_t>{}(interval_ms) << 2;
  h ^= std::hash<int64_t>{}(restart_interval_ms) << 3;
  return h;
}
```

#### Custom Module Constructor (Shared Mode)

```cpp
waybar::modules::Custom::Custom(const std::string& name, const std::string& id,
                                const Json::Value& config,
                                const std::string& output_name)
    : ALabel(config, "custom-" + name, id, "{}"),
      name_(name),
      output_name_(output_name),
      id_(id),
      shared_mode_(config["shared"].isBool() && config["shared"].asBool()) {

  if (shared_mode_) {
    worker_key_ = computeWorkerKey();
    CustomExecWorker::inst().subscribe(worker_key_, output_name_, this);
  } else {
    // Existing implementation (unchanged)
    if (!config_["signal"].empty() && config_["interval"].empty() &&
        config_["restart-interval"].empty()) {
      waitingWorker();
    } else if (interval_.count() > 0) {
      delayWorker();
    } else if (config_["exec"].isString()) {
      continuousWorker();
    }
  }
  dp.emit();
}

waybar::modules::Custom::~Custom() {
  if (shared_mode_) {
    CustomExecWorker::inst().unsubscribe(worker_key_, this);
  } else {
    // Existing cleanup code
    if (pid_ != -1) {
      killpg(pid_, SIGTERM);
      waitpid(pid_, NULL, 0);
      pid_ = -1;
    }
  }
}

void waybar::modules::Custom::onExecOutput(const util::command::res& output) {
  output_ = output;
  dp.emit();  // Trigger UI update
}
```

#### Worker Lifecycle Management

```cpp
void CustomExecWorker::subscribe(const WorkerKey& key,
                                  const std::string& output_name,
                                  CustomExecObserver* observer) {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  auto it = workers_.find(key);
  if (it == workers_.end()) {
    // First subscriber - create new worker
    auto state = std::make_unique<WorkerState>();
    state->key = key;
    state->output_name = output_name;

    workers_[key] = std::move(state);
    startWorker(key, output_name);
    it = workers_.find(key);
  }

  // Add observer to worker
  std::lock_guard<std::mutex> state_lock(it->second->mutex);
  it->second->observers.push_back(observer);

  // Immediately send last output if available
  if (!it->second->last_output.out.empty()) {
    observer->onExecOutput(it->second->last_output);
  }
}

void CustomExecWorker::unsubscribe(const WorkerKey& key,
                                    CustomExecObserver* observer) {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  auto it = workers_.find(key);
  if (it == workers_.end()) return;

  {
    std::lock_guard<std::mutex> state_lock(it->second->mutex);
    auto& observers = it->second->observers;
    observers.erase(std::remove(observers.begin(), observers.end(), observer),
                    observers.end());
  }

  // Last subscriber - clean up worker
  if (it->second->observers.empty()) {
    stopWorker(key);
    workers_.erase(it);
  }
}
```

#### Worker Thread Functions

The worker thread functions are almost identical to existing `Custom` module methods, but:
1. Store output in `WorkerState::last_output`
2. Call `state->notifyObservers()` instead of `dp.emit()`

```cpp
void CustomExecWorker::WorkerState::notifyObservers() {
  std::lock_guard<std::mutex> lock(mutex);
  for (auto* observer : observers) {
    observer->onExecOutput(last_output);
  }
}

void CustomExecWorker::runDelayWorker(WorkerState* state) {
  state->thread = [this, state] {
    // Similar to Custom::delayWorker() but:
    // 1. Use state->key instead of config_
    // 2. Store in state->last_output
    // 3. Call state->notifyObservers() instead of dp.emit()

    if (!state->key.exec_if.empty()) {
      auto output = util::command::execNoRead(state->key.exec_if);
      if (output.exit_code != 0) {
        state->last_output = output;
        state->notifyObservers();
        return;
      }
    }

    if (!state->key.exec.empty()) {
      state->last_output = util::command::exec(state->key.exec,
                                               state->output_name);
      state->notifyObservers();
    }

    state->thread.sleep_for(std::chrono::milliseconds(state->key.interval_ms));
  };
}

// Similar implementations for runContinuousWorker and runWaitingWorker
```

### Configuration Example

#### Before (duplicate execution):
```jsonc
{
  "modules-left": ["custom/weather"],
  "custom/weather": {
    "exec": "curl wttr.in/?format=3",
    "interval": 1800
  }
}
```
With 3 monitors: **3 API calls every 30 minutes**

#### After (shared execution):
```jsonc
{
  "modules-left": ["custom/weather"],
  "custom/weather": {
    "exec": "curl wttr.in/?format=3",
    "interval": 1800,
    "shared": true  // ← New opt-in flag
  }
}
```
With 3 monitors: **1 API call every 30 minutes**

## Benefits

1. **Resource Efficiency**: Single exec per unique command across all bars
2. **API Friendliness**: Reduces rate-limiting issues with external services
3. **Backward Compatible**: Existing configs work unchanged (default `shared: false`)
4. **Consistent Architecture**: Follows existing backend patterns (Hyprland IPC, etc.)
5. **Thread-Safe**: Proper mutex protection for shared state
6. **Clean Lifecycle**: Workers start on first subscription, stop on last unsubscribe

## Edge Cases & Considerations

### 1. Signal-based workers
For signal-based custom modules, signals need to wake the shared worker:
```cpp
void waybar::modules::Custom::refresh(int sig) {
  if (shared_mode_) {
    if (sig == SIGRTMIN + config_["signal"].asInt()) {
      CustomExecWorker::inst().wakeWorker(worker_key_);
    }
  } else {
    // Existing implementation
    if (sig == SIGRTMIN + config_["signal"].asInt()) {
      thread_.wake_up();
    }
  }
}
```

### 2. Different output names
The worker uses the `output_name` from the **first subscriber**. This is acceptable because:
- Most custom scripts don't use `$WAYBAR_OUTPUT_NAME`
- When they do, it's typically for per-output customization (which wouldn't use `shared: true`)

### 3. Module cleanup order
Workers are cleaned up when the last subscriber unsubscribes, which happens in module destructors. The singleton `CustomExecWorker` itself lives for the entire application lifetime.

### 4. User event handling (click/scroll)
With `exec-on-event`, only the clicked module should trigger execution in non-shared mode. In shared mode, the event wakes the shared worker, affecting all instances. This is documented behavior.

## Implementation Plan

### Phase 1: Core Infrastructure
1. Create `CustomExecWorker` singleton class
2. Implement `WorkerKey` and hashing
3. Add `CustomExecObserver` interface
4. Implement subscribe/unsubscribe logic

### Phase 2: Worker Thread Functions
1. Port `delayWorker()` to shared implementation
2. Port `continuousWorker()` to shared implementation
3. Port `waitingWorker()` to shared implementation
4. Implement observer notification

### Phase 3: Custom Module Integration
1. Add `shared` config flag detection
2. Modify constructor for shared mode
3. Implement `onExecOutput()` callback
4. Update destructor for unsubscribe
5. Handle signal refresh for shared workers

### Phase 4: Testing & Documentation
1. Test with multiple monitors
2. Test with different worker types (delay, continuous, signal)
3. Update man page with `shared` option
4. Add configuration examples

## Files to Modify

### New Files
- `include/modules/custom_exec_worker.hpp` - Worker backend header
- `src/modules/custom_exec_worker.cpp` - Worker backend implementation

### Modified Files
- `include/modules/custom.hpp` - Add shared mode members and observer interface
- `src/modules/custom.cpp` - Add shared mode logic
- `meson.build` - Add new source file
- `man/waybar-custom.5.scd` - Document `shared` option

### Build System
```meson
# In meson.build, add to src_files:
src_files = files(
    # ... existing files ...
    'src/modules/custom.cpp',
    'src/modules/custom_exec_worker.cpp',  # ← New
    # ... rest of files ...
)
```

## Backward Compatibility

✅ **Fully backward compatible**
- Default behavior unchanged (`shared: false` implicit)
- No breaking changes to existing configs
- No performance impact for non-shared modules
- Singleton worker backend has zero overhead until first shared module

## Alternative Approaches Considered

### 1. Module-level deduplication
**Rejected**: Requires tracking at Factory level, breaks encapsulation

### 2. Process-level shared worker daemon
**Rejected**: Over-engineered, requires IPC, harder to debug

### 3. Automatic detection (no config flag)
**Rejected**: Could break configs that intentionally run commands per-output

## Testing Strategy

### Unit Tests
- Worker key generation and hashing
- Subscribe/unsubscribe logic
- Observer notification

### Integration Tests
- Multi-bar scenario with shared modules
- Mixed shared and non-shared modules
- Signal-based shared workers
- Worker cleanup on module destruction

### Manual Testing
```bash
# Test setup: 3 monitors with identical custom module
# Config: shared: true, exec: "echo $RANDOM", interval: 5

# Expected: All 3 bars show same random number
# Expected: Numbers change every 5 seconds simultaneously
# Expected: Only 1 process running the command
```

## Documentation

### Man Page Addition (waybar-custom.5.scd)
```
*shared*: ++
	typeof: bool ++
	default: false ++
	When set to true, the exec command runs only once and its output is shared
	across all bars using this module definition. This is useful for resource-
	intensive scripts or rate-limited API calls. The worker is identified by
	the combination of exec, exec-if, interval, and restart-interval values.
	Note: When using shared mode with exec-on-event, events from any instance
	will trigger execution for all instances.
```

## Security Considerations

No new security concerns introduced:
- Uses existing `util::command::exec()` infrastructure
- Same process isolation as non-shared mode
- No additional privileges required
- Worker threads follow same safety patterns as existing code

## Performance Impact

- **Non-shared modules**: Zero overhead (code path unchanged)
- **Shared modules**: Minimal mutex contention (only during subscribe/unsubscribe)
- **Memory**: Small increase for observer lists and worker state
- **CPU**: Significant savings (N modules → 1 exec per unique command)

## Migration Guide

For users wanting to optimize existing configs:

1. Identify duplicate custom modules across bars
2. Add `"shared": true` to their config
3. Verify the command doesn't rely on output-specific environment variables
4. Test with multiple monitors

Example:
```diff
  "custom/vpn": {
    "exec": "~/scripts/vpn-status.sh",
-   "interval": 5
+   "interval": 5,
+   "shared": true
  }
```
