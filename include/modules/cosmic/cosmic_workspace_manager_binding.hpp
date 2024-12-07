#include "cosmic-workspace-unstable-v1-client-protocol.h"

namespace waybar::modules::cosmic {
void add_registry_listener(void *data);
void add_workspace_listener(zcosmic_workspace_handle_v1 *workspace_handle, void *data);
void add_workspace_group_listener(zcosmic_workspace_group_handle_v1 *workspace_group_handle,
                                  void *data);
zcosmic_workspace_manager_v1 *workspace_manager_bind(wl_registry *registry, uint32_t name,
                                                  uint32_t version, void *data);
}  // namespace waybar::modules::cosmic
