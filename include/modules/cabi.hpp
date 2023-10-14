#pragma once

#include <fmt/format.h>

#include <csignal>
#include <string>

#include "AModule.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class CABI : public AModule {
 public:
  CABI(const std::string&, const std::string&, const Json::Value&);
  virtual ~CABI();

 private:
  void* cabi_instance_ = nullptr;

  std::function<void*(GtkContainer*)> wbcabi_init_ = nullptr;
  std::function<void(void*)> wbcabi_deinit_ = [](void*) {};
  std::function<const char*(void*)> wbcabi_last_error_str_ = [](void*) { return nullptr; };
};

}  // namespace waybar::modules
