#pragma once

#include <string>

#include "AModule.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

extern "C" {
// C ABI representation of a config key/value pair
struct wbcffi_config_entry {
  const char* key;
  const char* value;
};
}

class CFFI : public AModule {
 public:
  CFFI(const std::string&, const std::string&, const Json::Value&);
  virtual ~CFFI();

  // virtual auto update() -> void override;
  // virtual operator Gtk::Widget&() override;

  virtual auto refresh(int signal) -> void override;
  virtual auto doAction(const std::string& name) -> void override;

 private:
  ///
  void* cffi_instance_ = nullptr;

  typedef void*(InitFn)(GtkContainer*, const struct wbcffi_config_entry* config_entries,
                        size_t config_entries_len);
  typedef void(DenitFn)(void* instance);
  typedef void(RefreshFn)(void* instance, int signal);
  typedef void(DoActionFn)(void* instance, const char* name);

  // FFI hooks
  struct {
    std::function<InitFn> init = nullptr;
    std::function<DenitFn> deinit = nullptr;
    std::function<RefreshFn> refresh = [](void*, int) {};
    std::function<DoActionFn> doAction = [](void*, const char*) {};
  } hooks_;
};

}  // namespace waybar::modules
