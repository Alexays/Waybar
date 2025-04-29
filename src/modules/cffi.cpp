#include "modules/cffi.hpp"

#include <dlfcn.h>
#include <json/value.h>

#include <algorithm>
#include <iostream>
#include <type_traits>

namespace waybar::modules {

CFFI::CFFI(const std::string& name, const std::string& id, const Json::Value& config)
    : AModule(config, name, id, true, true) {
  const auto dynlib_path = config_["module_path"].asString();
  if (dynlib_path.empty()) {
    throw std::runtime_error{"Missing or empty 'module_path' in module config"};
  }

  void* handle = dlopen(dynlib_path.c_str(), RTLD_LAZY);
  if (handle == nullptr) {
    throw std::runtime_error{std::string{"Failed to load CFFI module: "} + dlerror()};
  }

  // Fetch ABI version
  auto wbcffi_version = reinterpret_cast<size_t*>(dlsym(handle, "wbcffi_version"));
  if (wbcffi_version == nullptr) {
    throw std::runtime_error{std::string{"Missing wbcffi_version function: "} + dlerror()};
  }

  // Fetch functions
  if (*wbcffi_version == 1 || *wbcffi_version == 2) {
    // Mandatory functions
    hooks_.init = reinterpret_cast<InitFn*>(dlsym(handle, "wbcffi_init"));
    if (!hooks_.init) {
      throw std::runtime_error{std::string{"Missing wbcffi_init function: "} + dlerror()};
    }
    hooks_.deinit = reinterpret_cast<DenitFn*>(dlsym(handle, "wbcffi_deinit"));
    if (!hooks_.init) {
      throw std::runtime_error{std::string{"Missing wbcffi_deinit function: "} + dlerror()};
    }
    // Optional functions
    if (auto fn = reinterpret_cast<UpdateFn*>(dlsym(handle, "wbcffi_update"))) {
      hooks_.update = fn;
    }
    if (auto fn = reinterpret_cast<RefreshFn*>(dlsym(handle, "wbcffi_refresh"))) {
      hooks_.refresh = fn;
    }
    if (auto fn = reinterpret_cast<DoActionFn*>(dlsym(handle, "wbcffi_doaction"))) {
      hooks_.doAction = fn;
    }
  } else {
    throw std::runtime_error{"Unknown wbcffi_version " + std::to_string(*wbcffi_version)};
  }

  // Prepare init() arguments
  // Convert JSON values to string
  std::vector<std::string> config_entries_stringstor;
  const auto& keys = config.getMemberNames();
  for (size_t i = 0; i < keys.size(); i++) {
    const auto& value = config[keys[i]];
    if (*wbcffi_version == 1) {
      if (value.isConvertibleTo(Json::ValueType::stringValue)) {
        config_entries_stringstor.push_back(value.asString());
      } else {
        config_entries_stringstor.push_back(value.toStyledString());
      }
    } else {
      config_entries_stringstor.push_back(value.toStyledString());
    }
  }

  // Prepare config_entries array
  std::vector<ffi::wbcffi_config_entry> config_entries;
  for (size_t i = 0; i < keys.size(); i++) {
    config_entries.push_back({keys[i].c_str(), config_entries_stringstor[i].c_str()});
  }

  ffi::wbcffi_init_info init_info = {
      .obj = (ffi::wbcffi_module*)this,
      .waybar_version = VERSION,
      .get_root_widget =
          [](ffi::wbcffi_module* obj) {
            return dynamic_cast<Gtk::Container*>(&((CFFI*)obj)->event_box_)->gobj();
          },
      .queue_update = [](ffi::wbcffi_module* obj) { ((CFFI*)obj)->dp.emit(); },
  };

  // Call init
  cffi_instance_ = hooks_.init(&init_info, config_entries.data(), config_entries.size());

  // Handle init failures
  if (cffi_instance_ == nullptr) {
    throw std::runtime_error{"Failed to initialize C ABI module"};
  }
}

CFFI::~CFFI() {
  if (cffi_instance_ != nullptr) {
    hooks_.deinit(cffi_instance_);
  }
}

auto CFFI::update() -> void {
  assert(cffi_instance_ != nullptr);
  hooks_.update(cffi_instance_);

  // Execute the on-update command set in config
  AModule::update();
}

auto CFFI::refresh(int signal) -> void {
  assert(cffi_instance_ != nullptr);
  hooks_.refresh(cffi_instance_, signal);
}

auto CFFI::doAction(const std::string& name) -> void {
  assert(cffi_instance_ != nullptr);
  if (!name.empty()) {
    hooks_.doAction(cffi_instance_, name.c_str());
  }
}

}  // namespace waybar::modules
