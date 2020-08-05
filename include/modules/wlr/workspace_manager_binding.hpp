#include "wlr-workspace-unstable-v1-client-protocol.h"

namespace waybar::modules::wlr {
  void add_registry_listener(void *data);
  void add_workspace_listener(zwlr_workspace_handle_v1 *workspace_handle, void *data);
  void add_workspace_group_listener(zwlr_workspace_group_handle_v1 *workspace_group_handle, void *data);
  zwlr_workspace_manager_v1* workspace_manager_bind(wl_registry *registry, uint32_t name, uint32_t version, void *data);
}