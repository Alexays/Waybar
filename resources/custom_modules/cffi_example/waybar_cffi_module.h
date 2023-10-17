#pragma once

#include <gtk/gtk.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Waybar ABI version. 1 is the latest version
extern const size_t wbcffi_version;

/// Config key-value pair
struct wbcffi_config_entry {
  /// Entry key
  const char* key;
  /// Entry value as string. JSON object and arrays are serialized.
  const char* value;
};

/// Module init/new function, called on module instantiation
///
/// MANDATORY CFFI function
///
/// @param root_widget        Root GTK widget instantiated by Waybar
/// @param trigger_update     Call this function with trigger_update_arg as argument to trigger
///                           wbcffi_update() on the next GTK main event loop iteration
/// @param trigger_update_arg Argument for trigger_update call
/// @param config_entries     Flat representation of the module JSON config. The data only available
///                           during wbcffi_init call.
/// @param config_entries_len Number of entries in `config_entries`
///
/// @return A untyped pointer to module data, NULL if the module failed to load.
void* wbcffi_init(GtkContainer* root_widget, const void (*trigger_update)(void*),
                  void* trigger_update_arg, const struct wbcffi_config_entry* config_entries,
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
