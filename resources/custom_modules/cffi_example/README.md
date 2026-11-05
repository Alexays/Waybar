# C FFI module

A C FFI module is a dynamic library that exposes standard C functions and
constants, that Waybar can load and execute to create custom advanced widgets.

Most language can implement the required functions and constants (C, C++, Rust,
Go, Python, ...), meaning you can develop custom modules using your language of
choice, as long as there's GTK bindings.

Symbols to implement are documented in the
[waybar_cffi_module.h](waybar_cffi_module.h) file.

Waybar uses GTK4: a module must link GTK4 and set `wbcffi_version` to 3. Libraries
built for an older `wbcffi_version` link GTK3 and are refused.

## Porting a module from wbcffi_version 2

- Build against `gtk4` instead of `gtk+-3.0`, and port the module's own widgets to
  GTK4.
- Set `wbcffi_version` to 3. Config values are passed as JSON, like in version 2.
- `wbcffi_init_info::get_root_widget` is replaced by `set_root_widget`: instead of
  adding a widget to the returned container with
  `gtk_container_add(GTK_CONTAINER(root), widget)`, call
  `init_info->set_root_widget(init_info->obj, widget)`. The module displays a single
  widget, so use a container such as a `GtkBox` to show several.

# Usage

## Building this module

```bash
meson setup build
meson compile -C build
```

## Load the module

Edit your waybar config:
```json
{
	// ...
	"modules-center": [
		// ...
		"cffi/c_example"
	],
	// ...
	"cffi/c_example": {
		// Path to the compiled dynamic library file
		"module_path": "resources/custom_modules/cffi_example/build/wb_cffi_example.so"
	}
}
```
