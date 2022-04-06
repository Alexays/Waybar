#include "ext-workspace-unstable-v1-client-protocol.h"

namespace waybar::modules::wlr {
void add_registry_listener(void *data);
void add_workspace_listener(zext_workspace_handle_v1 *workspace_handle, void *data);
void add_workspace_group_listener(zext_workspace_group_handle_v1 *workspace_group_handle,
                                  void *data);
zext_workspace_manager_v1 *workspace_manager_bind(wl_registry *registry, uint32_t name,
                                                  uint32_t version, void *data);
}  // namespace waybar::modules::wlr
