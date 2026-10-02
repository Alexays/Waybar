waybar-hyprland-workbar(5)

# NAME

waybar - Hyprland workbar module

# DESCRIPTION

The *workbar* module combines the functionality of the traditional *workspaces* and *taskbar* modules into a single component.

Each workspace is displayed as a single pill containing its workspace number along with icons representing the windows currently opened on that workspace.

The module communicates directly with Hyprland through Waybar's native IPC backend and does not require any external helper scripts.

The module is intended for users who prefer a workspace-centric workflow by grouping workspace navigation and window management into a single interface.

# CONFIGURATION

Addressed by *hyprland/workbar*

*rotate*: ++
	typeof: integer ++
	default: 0 ++
	Rotates the module.

# FEATURES

- Unified replacement for Workspaces and Taskbar modules
- Displays application icons grouped by workspace
- Highlights the active workspace
- Highlights the active window
- Indicates workspaces visible on non-focused monitors
- Drag and drop windows between workspaces
- Middle-click window icons to close windows
- Native Hyprland IPC implementation
- Incremental widget updates for improved performance

# INTERACTION

Left-click workspace number::
	Switch to the selected workspace.

Left-click window icon::
	Switch to the corresponding workspace (if necessary) and focus the selected window.

Middle-click window icon::
	Close the selected window.

Drag window icon::
	Move the window to another workspace.

# EXAMPLE

```
"hyprland/workbar": {
	"rotate": 0
}
```

# STYLE

The following CSS classes are available:

- *#workbar*
- *.workspace*
- *.workspace-number*
- *.workspace-number.active*
- *.workspace-number.visible*
- *.window-icon*
- *.window-icon.active*

# SEE ALSO

*waybar*(5)

*waybar-hyprland-workspaces*(5)

*waybar-hyprland-window*(5)