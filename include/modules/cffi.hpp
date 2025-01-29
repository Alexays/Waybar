#pragma once

#include <gtkmm/box.h>

#include "AModule.hpp"

namespace waybar::modules {

namespace ffi {
extern "C" {
using wbcffi_module = struct wbcffi_module;

using wbcffi_init_info = struct {
  wbcffi_module* obj;
  const char* waybar_version;
  GtkBox* (*get_root_widget)(wbcffi_module*);
  void (*queue_update)(wbcffi_module*);
};

struct wbcffi_config_entry {
  const char* key;
  const char* value;
};
}
}  // namespace ffi

class CFFI final : public AModule {
 public:
  CFFI(const std::string&, const std::string&, const Json::Value&);
  virtual ~CFFI();

  virtual auto refresh(int signal) -> void override;
  virtual auto doAction(const std::string& name) -> void override;
  virtual auto update() -> void override;
  Gtk::Widget& root() override;

 private:
  ///
  void* cffi_instance_ = nullptr;
  Gtk::Box box_;

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
