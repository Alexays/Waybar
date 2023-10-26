#pragma once

#include <pipewire/pipewire.h>

#include "util/backend_common.hpp"
#include "util/pipewire/privacy_node_info.hpp"

namespace waybar::util::PipewireBackend {

class PipewireBackend {
 private:
  pw_thread_loop* mainloop_;
  pw_context* context_;
  pw_core* core_;

  spa_hook registry_listener;

  /* Hack to keep constructor inaccessible but still public.
   * This is required to be able to use std::make_shared.
   * It is important to keep this class only accessible via a reference-counted
   * pointer because the destructor will manually free memory, and this could be
   * a problem with C++20's copy and move semantics.
   */
  struct private_constructor_tag {};

 public:
  std::mutex mutex_;

  pw_registry* registry;

  sigc::signal<void> privacy_nodes_changed_signal_event;

  std::unordered_map<uint32_t, PrivacyNodeInfo*> privacy_nodes;

  static std::shared_ptr<PipewireBackend> getInstance();

  PipewireBackend(private_constructor_tag tag);
  ~PipewireBackend();
};
}  // namespace waybar::util::pipewire::PipewireBackend
