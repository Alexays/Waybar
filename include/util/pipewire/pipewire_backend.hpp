#pragma once

#include <pipewire/pipewire.h>

#include <unordered_map>

#include "util/backend_common.hpp"
#include "util/pipewire/privacy_node_info.hpp"

namespace waybar::util::PipewireBackend {

class PipewireBackend {
 private:
  pw_thread_loop* mainloop_;
  pw_context* context_;
  pw_core* core_;

  pw_registry* registry_;
  spa_hook registryListener_;

  /* Hack to keep constructor inaccessible but still public.
   * This is required to be able to use std::make_shared.
   * It is important to keep this class only accessible via a reference-counted
   * pointer because the destructor will manually free memory, and this could be
   * a problem with C++20's copy and move semantics.
   */
  struct PrivateConstructorTag {};

 public:
  sigc::signal<void> privacy_nodes_changed_signal_event;

  std::unordered_map<uint32_t, PrivacyNodeInfo*> privacy_nodes;
  std::mutex mutex_;

  static std::shared_ptr<PipewireBackend> getInstance();

  // Handlers for PipeWire events
  void handleRegistryEventGlobal(uint32_t id, uint32_t permissions, const char* type,
                                 uint32_t version, const struct spa_dict* props);
  void handleRegistryEventGlobalRemove(uint32_t id);

  PipewireBackend(PrivateConstructorTag tag);
  ~PipewireBackend();
};
}  // namespace waybar::util::PipewireBackend
