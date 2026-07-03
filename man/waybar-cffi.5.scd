waybar-cffi(5)
# NAME

waybar - cffi module

# DESCRIPTION

The *cffi* module gives full control of a GTK widget to a third-party dynamic library, to create more complex modules using different programming languages.

# CONFIGURATION

Addressed by *cffi/<name>*

*module_path*: ++
	typeof: string ++
	The path to the dynamic library to load to control the widget.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

Some additional configuration may be required depending on the cffi dynamic library being used.


# EXAMPLES

## C example:

An example module written in C can be found at https://github.com/Alexays/Waybar/resources/custom_modules/cffi_example/

Waybar config to enable the module:
```
"cffi/c_example": {
	"module_path": ".config/waybar/cffi/wb_cffi_example.so"
}
```


# STYLE

The classes and IDs are managed by the cffi dynamic library.
