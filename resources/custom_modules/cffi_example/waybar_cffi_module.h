#pragma once

#include <gtk/gtk.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Waybar ABI version. 2 is the latest version
extern const size_t wbcffi_version;

/// Private Waybar CFFI module
typedef struct wbcffi_module wbcffi_module;

/// Waybar module information
typedef struct {
  /// Waybar CFFI object pointer
  wbcffi_module* obj;

  /// Waybar version string
  const char* waybar_version;

  /// Returns the waybar widget allocated for this module
  /// @param obj Waybar CFFI object pointer
  GtkContainer* (*get_root_widget)(wbcffi_module* obj);

  /// Queues a request for calling wbcffi_update() on the next GTK main event
  /// loop iteration
  /// @param obj Waybar CFFI object pointer
  void (*queue_update)(wbcffi_module*);
} wbcffi_init_info;

/// Config key-value pair
typedef struct {
  /// Entry key
  const char* key;
  /// Entry value
  ///
  /// In ABI version 1, this may be either a bare string if the value is a
  /// string, or the JSON representation of any other JSON object as a string.
  ///
  /// From ABI version 2 onwards, this is always the JSON representation of the
  /// value as a string.
  const char* value;
} wbcffi_config_entry;

/// Module init/new function, called on module instantiation
///
/// MANDATORY CFFI function
///
/// @param init_info          Waybar module information
/// @param config_entries     Flat representation of the module JSON config. The data only available
///                           during wbcffi_init call.
/// @param config_entries_len Number of entries in `config_entries`
///
/// @return A untyped pointer to module data, NULL if the module failed to load.
void* wbcffi_init(const wbcffi_init_info* init_info, const wbcffi_config_entry* config_entries,
                  size_t config_entries_len);

/// Module deinit/delete function, called when Waybar is closed or when the module is removed
///
/// MANDATORY CFFI function
///
/// @param instance Module instance data (as returned by `wbcffi_init`)
void wbcffi_deinit(void* instance);

/// Called from the GTK main event loop, to update the UI
///
/// Optional CFFI function
///
/// @param instance Module instance data (as returned by `wbcffi_init`)
/// @param action_name Action name
void wbcffi_update(void* instance);

/// Called when Waybar receives a POSIX signal and forwards it to each module
///
/// Optional CFFI function
///
/// @param instance Module instance data (as returned by `wbcffi_init`)
/// @param signal Signal ID
void wbcffi_refresh(void* instance, int signal);

/// Called on module action (see
/// https://github.com/Alexays/Waybar/wiki/Configuration#module-actions-config)
///
/// Optional CFFI function
///
/// @param instance Module instance data (as returned by `wbcffi_init`)
/// @param action_name Action name
void wbcffi_doaction(void* instance, const char* action_name);

#ifdef __cplusplus
}
#endif
