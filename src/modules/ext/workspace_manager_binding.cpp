#include "modules/ext/workspace_manager_binding.hpp"

#include <spdlog/spdlog.h>

#include <cstdint>

#include "client.hpp"
#include "modules/ext/workspace_manager.hpp"

namespace waybar::modules::ext {

static void handle_global(void *data, wl_registry *registry, uint32_t name, const char *interface,
                          uint32_t version) {
  if (std::strcmp(interface, ext_workspace_manager_v1_interface.name) == 0) {
    static_cast<WorkspaceManager *>(data)->register_manager(registry, name, version);
  }
}

static void handle_global_remove(void *data, wl_registry *registry, uint32_t name) {
  /* Nothing to do here */
}

static const wl_registry_listener registry_listener_impl = {.global = handle_global,
                                                            .global_remove = handle_global_remove};

void add_registry_listener(void *data) {
  wl_display *display = Client::inst()->wl_display;
  wl_registry *registry = wl_display_get_registry(display);

  wl_registry_add_listener(registry, &registry_listener_impl, data);
  wl_display_roundtrip(display);
}

static void workspace_manager_handle_workspace_group(
    void *data, ext_workspace_manager_v1 *_, ext_workspace_group_handle_v1 *workspace_group) {
  static_cast<WorkspaceManager *>(data)->handle_workspace_group(workspace_group);
}

static void workspace_manager_handle_workspace(void *data, ext_workspace_manager_v1 *_,
                                               ext_workspace_handle_v1 *workspace) {
  static_cast<WorkspaceManager *>(data)->handle_workspace(workspace);
}

static void workspace_manager_handle_done(void *data, ext_workspace_manager_v1 *_) {
  static_cast<WorkspaceManager *>(data)->handle_done();
}

static void workspace_manager_handle_finished(void *data, ext_workspace_manager_v1 *_) {
  static_cast<WorkspaceManager *>(data)->handle_finished();
}

static const ext_workspace_manager_v1_listener workspace_manager_impl = {
    .workspace_group = workspace_manager_handle_workspace_group,
    .workspace = workspace_manager_handle_workspace,
    .done = workspace_manager_handle_done,
    .finished = workspace_manager_handle_finished,
};

ext_workspace_manager_v1 *workspace_manager_bind(wl_registry *registry, uint32_t name,
                                                 uint32_t version, void *data) {
  auto *workspace_manager = static_cast<ext_workspace_manager_v1 *>(
      wl_registry_bind(registry, name, &ext_workspace_manager_v1_interface, version));

  if (workspace_manager)
    ext_workspace_manager_v1_add_listener(workspace_manager, &workspace_manager_impl, data);
  else
    spdlog::error("Failed to register manager");

  return workspace_manager;
}

static void workspace_group_handle_capabilities(void *data, ext_workspace_group_handle_v1 *_,
                                                uint32_t capabilities) {
  static_cast<WorkspaceGroup *>(data)->handle_capabilities(capabilities);
}

static void workspace_group_handle_output_enter(void *data, ext_workspace_group_handle_v1 *_,
                                                wl_output *output) {
  static_cast<WorkspaceGroup *>(data)->handle_output_enter(output);
}

static void workspace_group_handle_output_leave(void *data, ext_workspace_group_handle_v1 *_,
                                                wl_output *output) {
  static_cast<WorkspaceGroup *>(data)->handle_output_leave(output);
}

static void workspace_group_handle_workspace_enter(void *data, ext_workspace_group_handle_v1 *_,
                                                   ext_workspace_handle_v1 *workspace) {
  static_cast<WorkspaceGroup *>(data)->handle_workspace_enter(workspace);
}

static void workspace_group_handle_workspace_leave(void *data, ext_workspace_group_handle_v1 *_,
                                                   ext_workspace_handle_v1 *workspace) {
  static_cast<WorkspaceGroup *>(data)->handle_workspace_leave(workspace);
}

static void workspace_group_handle_removed(void *data, ext_workspace_group_handle_v1 *_) {
  static_cast<WorkspaceGroup *>(data)->handle_removed();
}

static const ext_workspace_group_handle_v1_listener workspace_group_impl = {
    .capabilities = workspace_group_handle_capabilities,
    .output_enter = workspace_group_handle_output_enter,
    .output_leave = workspace_group_handle_output_leave,
    .workspace_enter = workspace_group_handle_workspace_enter,
    .workspace_leave = workspace_group_handle_workspace_leave,
    .removed = workspace_group_handle_removed};

void add_workspace_group_listener(ext_workspace_group_handle_v1 *workspace_group_handle,
                                  void *data) {
  ext_workspace_group_handle_v1_add_listener(workspace_group_handle, &workspace_group_impl, data);
}

void workspace_handle_name(void *data, struct ext_workspace_handle_v1 *_, const char *name) {
  static_cast<Workspace *>(data)->handle_name(name);
}

void workspace_handle_id(void *data, struct ext_workspace_handle_v1 *_, const char *id) {
  static_cast<Workspace *>(data)->handle_id(id);
}

void workspace_handle_coordinates(void *data, struct ext_workspace_handle_v1 *_,
                                  struct wl_array *coordinates) {
  std::vector<uint32_t> coords_vec;
  auto coords = static_cast<uint32_t *>(coordinates->data);
  for (size_t i = 0; i < coordinates->size / sizeof(uint32_t); ++i) {
    coords_vec.push_back(coords[i]);
  }

  static_cast<Workspace *>(data)->handle_coordinates(coords_vec);
}

void workspace_handle_state(void *data, struct ext_workspace_handle_v1 *workspace_handle,
                            uint32_t state) {
  static_cast<Workspace *>(data)->handle_state(state);
}

static void workspace_handle_capabilities(void *data,
                                          struct ext_workspace_handle_v1 *workspace_handle,
                                          uint32_t capabilities) {
  static_cast<Workspace *>(data)->handle_capabilities(capabilities);
}

void workspace_handle_removed(void *data, struct ext_workspace_handle_v1 *workspace_handle) {
  static_cast<Workspace *>(data)->handle_removed();
}

static const ext_workspace_handle_v1_listener workspace_impl = {
    .id = workspace_handle_id,
    .name = workspace_handle_name,
    .coordinates = workspace_handle_coordinates,
    .state = workspace_handle_state,
    .capabilities = workspace_handle_capabilities,
    .removed = workspace_handle_removed};

void add_workspace_listener(ext_workspace_handle_v1 *workspace_handle, void *data) {
  ext_workspace_handle_v1_add_listener(workspace_handle, &workspace_impl, data);
}
}  // namespace waybar::modules::ext
