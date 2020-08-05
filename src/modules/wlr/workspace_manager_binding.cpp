#include "modules/wlr/workspace_manager_binding.hpp"

#include <spdlog/spdlog.h>

#include "client.hpp"
#include "modules/wlr/workspace_manager.hpp"

namespace waybar::modules::wlr {

static void handle_global(void *data, wl_registry *registry, uint32_t name, const char *interface,
                          uint32_t version) {
  if (std::strcmp(interface, zwlr_workspace_manager_v1_interface.name) == 0) {
    static_cast<WorkspaceManager *>(data)->register_manager(registry, name, version);
  }
}

static void handle_global_remove(void *data, wl_registry *registry, uint32_t name) {
  /* Nothing to do here */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

void add_registry_listener(void *data) {
  wl_display * display = Client::inst()->wl_display;
  wl_registry *registry = wl_display_get_registry(display);

  wl_registry_add_listener(registry, &registry_listener_impl, data);
  wl_display_roundtrip(display);
  wl_display_roundtrip(display);
}

static void workspace_manager_handle_workspace_group(
    void *data, zwlr_workspace_manager_v1 *_, zwlr_workspace_group_handle_v1 *workspace_group) {
  static_cast<WorkspaceManager *>(data)->handle_workspace_group_create(workspace_group);
}

static void workspace_manager_handle_done(void *data, zwlr_workspace_manager_v1 *_) {
  static_cast<WorkspaceManager *>(data)->handle_done();
}

static void workspace_manager_handle_finished(void *data, zwlr_workspace_manager_v1 *_) {
  static_cast<WorkspaceManager *>(data)->handle_finished();
}

static const zwlr_workspace_manager_v1_listener workspace_manager_impl = {
    .workspace_group = workspace_manager_handle_workspace_group,
    .done = workspace_manager_handle_done,
    .finished = workspace_manager_handle_finished,
};

zwlr_workspace_manager_v1 *workspace_manager_bind(wl_registry *registry, uint32_t name,
                                                  uint32_t version, void *data) {
  auto *workspace_manager = static_cast<zwlr_workspace_manager_v1 *>(
      wl_registry_bind(registry, name, &zwlr_workspace_manager_v1_interface, version));

  if (workspace_manager)
    zwlr_workspace_manager_v1_add_listener(workspace_manager, &workspace_manager_impl, data);
  else
    spdlog::error("Failed to register manager");

  return workspace_manager;
}

static void workspace_group_handle_output_enter(void *data, zwlr_workspace_group_handle_v1 *_,
                                                wl_output *output) {
  static_cast<WorkspaceGroup *>(data)->handle_output_enter(output);
}

static void workspace_group_handle_output_leave(void *data, zwlr_workspace_group_handle_v1 *_,
                                                wl_output *output) {
  static_cast<WorkspaceGroup *>(data)->handle_output_leave();
}

static void workspace_group_handle_workspace(void *data, zwlr_workspace_group_handle_v1 *_,
                                             zwlr_workspace_handle_v1 *workspace) {
  static_cast<WorkspaceGroup *>(data)->handle_workspace_create(workspace);
}

static void workspace_group_handle_remove(void *data, zwlr_workspace_group_handle_v1 *_) {
  static_cast<WorkspaceGroup *>(data)->handle_remove();
}

static const zwlr_workspace_group_handle_v1_listener workspace_group_impl = {
    .output_enter = workspace_group_handle_output_enter,
    .output_leave = workspace_group_handle_output_leave,
    .workspace = workspace_group_handle_workspace,
    .remove = workspace_group_handle_remove};

void add_workspace_group_listener(zwlr_workspace_group_handle_v1 *workspace_group_handle,
                                  void *                          data) {
  zwlr_workspace_group_handle_v1_add_listener(workspace_group_handle, &workspace_group_impl, data);
}

void workspace_handle_name(void *data, struct zwlr_workspace_handle_v1 *_, const char *name) {
  static_cast<Workspace *>(data)->handle_name(name);
}

void workspace_handle_coordinates(void *data, struct zwlr_workspace_handle_v1 *_,
                                  struct wl_array *coordinates) {
  std::vector<uint32_t> coords_vec;
  auto                  coords = static_cast<uint32_t *>(coordinates->data);
  for (size_t i = 0; i < coordinates->size; ++i) {
    coords_vec.push_back(coords[i]);
  }

  static_cast<Workspace *>(data)->handle_coordinates(coords_vec);
}

void workspace_handle_state(void *data, struct zwlr_workspace_handle_v1 *_,
                            struct wl_array *state) {
  std::vector<uint32_t> state_vec;
  auto                  coords = static_cast<uint32_t *>(state->data);
  for (size_t i = 0; i < state->size; ++i) {
    state_vec.push_back(coords[i]);
  }

  static_cast<Workspace *>(data)->handle_state(state_vec);
}

void workspace_handle_remove(void *data, struct zwlr_workspace_handle_v1 *_) {
  static_cast<Workspace *>(data)->handle_remove();
}

static const zwlr_workspace_handle_v1_listener workspace_impl = {
    .name = workspace_handle_name,
    .coordinates = workspace_handle_coordinates,
    .state = workspace_handle_state,
    .remove = workspace_handle_remove};

void add_workspace_listener(zwlr_workspace_handle_v1 *workspace_handle, void *data) {
  zwlr_workspace_handle_v1_add_listener(workspace_handle, &workspace_impl, data);
}
}  // namespace waybar::modules::wlr
