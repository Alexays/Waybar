#include "modules/cabi.hpp"

#include <dlfcn.h>

namespace waybar::modules {

CABI::CABI(const std::string& name, const std::string& id, const Json::Value& config)
    : AModule(config, name, id, true, true) {
  const auto dynlib_path = config_["path"].asString();

  void* handle = dlopen(dynlib_path.c_str(), RTLD_LAZY);
  if (handle == nullptr) {
    throw std::runtime_error{dlerror()};
  }

  // Fetch ABI version
  auto wbcabi_version = reinterpret_cast<size_t*>(dlsym(handle, "wbcabi_version"));
  if (wbcabi_version == nullptr) {
    throw std::runtime_error{"Missing wbcabi_version"};
  }

  // Fetch functions
  if (*wbcabi_version == 1) {
    wbcabi_init_ = reinterpret_cast<void* (*)(GtkContainer*)>(dlsym(handle, "wbcabi_init"));
    if (wbcabi_init_ == nullptr) {
      throw std::runtime_error{"Missing wbcabi_init function"};
    }
    if (auto fn = reinterpret_cast<void (*)(void*)>(dlsym(handle, "wbcabi_deinit")))
      wbcabi_deinit_ = fn;
    if (auto fn = reinterpret_cast<const char* (*)(void*)>(dlsym(handle, "wbcabi_last_error_str")))
      wbcabi_last_error_str_ = fn;
  } else {
    throw std::runtime_error{"Unknown wbcabi_version " + std::to_string(*wbcabi_version)};
  }

  cabi_instance_ = wbcabi_init_(dynamic_cast<Gtk::Container*>(&event_box_)->gobj());
  if (cabi_instance_ == nullptr) {
    const auto err_str = wbcabi_last_error_str_(cabi_instance_);
    throw std::runtime_error{std::string{"Failed to initialize C ABI plugin: "} +
                             (err_str != nullptr ? err_str : "unknown error")};
  }
}

CABI::~CABI() {
  if (cabi_instance_ != nullptr) {
    wbcabi_deinit_(cabi_instance_);
  }
}

}  // namespace waybar::modules
