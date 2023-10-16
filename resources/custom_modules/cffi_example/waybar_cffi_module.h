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
/// @param config_entries     Flat representation of the module JSON config. The data only available
///                           during wbcffi_init call.
/// @param config_entries_len Number of entries in `config_entries`
///
/// @return A untyped pointer to module data, NULL if the module failed to load.
void* wbcffi_init(GtkContainer* root_widget, const struct wbcffi_config_entry* config_entries,
                  size_t config_entries_len);

/// Module deinit/delete function, called when Waybar is closed or when the module is removed
///
/// MANDATORY CFFI function
///
/// @param instance Module instance data (as returned by `wbcffi_init`)
void wbcffi_deinit(void* instance);

/// When Waybar receives a POSIX signal, it forwards the signal to each module, calling this
/// function
///
/// Optional CFFI function
///
/// @param instance Module instance data (as returned by `wbcffi_init`)
/// @param signal Signal ID
void wbcffi_refresh(void* instance, int signal);

///
/// Optional CFFI function
///
/// @param instance Module instance data (as returned by `wbcffi_init`)
/// @param name Action name
void wbcffi_doaction(void* instance, const char* name);

#ifdef __cplusplus
}
#endif
