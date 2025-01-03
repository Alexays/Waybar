waybar-hyprland-windowcount(5)

# NAME

waybar - hyprland window count module

# DESCRIPTION

The *windowcount* module displays the number of windows in the current Hyprland workspace.

# CONFIGURATION

Addressed by *hyprland/windowcount*

*format*: ++
	typeof: string ++
	default: {} ++
	The format for how information should be displayed. On {} the current workspace window count is displayed.

*format-empty*: ++
	typeof: string ++
	Override the format when the workspace contains no windows window

*format-windowed*: ++
	typeof: string ++
	Override the format when the workspace contains no fullscreen windows

*format-fullscreen*: ++
	typeof: string ++
	Override the format when the workspace contains a fullscreen window

*separate-outputs*: ++
	typeof: bool ++
	default: true ++
	Show the active workspace window count of the monitor the bar belongs to, instead of the focused workspace.

# STYLE

- *#windowcount*

The following classes are applied to the entire Waybar rather than just the
windowcount widget:

- *window#waybar.empty* When no windows are in the workspace
- *window#waybar.fullscreen* When there is a fullscreen window in the workspace;
  useful with Hyprland's *fullscreen, 1* mode
