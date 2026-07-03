# C FFI module

A C FFI module is a dynamic library that exposes standard C functions and
constants, that Waybar can load and execute to create custom advanced widgets.

Most language can implement the required functions and constants (C, C++, Rust,
Go, Python, ...), meaning you can develop custom modules using your language of
choice, as long as there's GTK bindings.

Symbols to implement are documented in the
[waybar_cffi_module.h](waybar_cffi_module.h) file.

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
