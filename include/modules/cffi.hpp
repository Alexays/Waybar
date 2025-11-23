#pragma once

#include <string>

#include "AModule.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

namespace ffi {
extern "C" {
typedef struct wbcffi_module wbcffi_module;

typedef struct {
  wbcffi_module* obj;
  const char* waybar_version;
  GtkContainer* (*get_root_widget)(wbcffi_module*);
  void (*queue_update)(wbcffi_module*);
} wbcffi_init_info;

struct wbcffi_config_entry {
  const char* key;
  const char* value;
};
}
}  // namespace ffi

class CFFI : public AModule {
 public:
  CFFI(const std::string&, const std::string&, const Json::Value&,
       std::mutex& reap_mtx, std::list<pid_t>& reap);
  virtual ~CFFI();

  virtual auto refresh(int signal) -> void override;
  virtual auto doAction(const std::string& name) -> void override;
  virtual auto update() -> void override;

 private:
  ///
  void* cffi_instance_ = nullptr;

  typedef void*(InitFn)(const ffi::wbcffi_init_info* init_info,
                        const ffi::wbcffi_config_entry* config_entries, size_t config_entries_len);
  typedef void(DenitFn)(void* instance);
  typedef void(RefreshFn)(void* instance, int signal);
  typedef void(DoActionFn)(void* instance, const char* name);
  typedef void(UpdateFn)(void* instance);

  // FFI hooks
  struct {
    std::function<InitFn> init = nullptr;
    std::function<DenitFn> deinit = nullptr;
    std::function<RefreshFn> refresh = [](void*, int) {};
    std::function<DoActionFn> doAction = [](void*, const char*) {};
    std::function<UpdateFn> update = [](void*) {};
  } hooks_;
};

}  // namespace waybar::modules
