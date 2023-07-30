waybar-hyprland-window(5)

# NAME

waybar - hyprland window module

# DESCRIPTION

The *window* module displays the title of the currently focused window in Hyprland.

# CONFIGURATION

Addressed by *hyprland/window*

*format*: ++
	typeof: string ++
	default: {title} ++
	The format, how information should be displayed. On {} the current window title is displayed.

*rewrite*: ++
	typeof: object ++
	Rules to rewrite window title. See *rewrite rules*.

*separate-outputs*: ++
	typeof: bool ++
	Show the active window of the monitor the bar belongs to, instead of the focused window.

*icon*: ++
	typeof: bool ++
	default: false ++
	Option to hide the application icon.

*icon-size*: ++
	typeof: integer ++
	default: 24 ++
	Option to change the size of the application icon.

# FORMAT REPLACEMENTS
See the output of "hyprctl clients" for examples

*{title}*: The current title of the focused window.

*{initialTitle}*: The initial title of the focused window.

*{class}*: The current class of the focused window.

*{initialClass}*: The initial class of the focused window.

# REWRITE RULES

*rewrite* is an object where keys are regular expressions and values are
rewrite rules if the expression matches. Rules may contain references to
captures of the expression.

Regular expression and replacement follow ECMA-script rules.

If no expression matches, the title is left unchanged.

Invalid expressions (e.g., mismatched parentheses) are skipped.

# EXAMPLES

```
"hyprland/window": {
	"format": "{}",
	"rewrite": {
		"(.*) - Mozilla Firefox": "🌎 $1",
		"(.*) - zsh": "> [$1]"
	}
}
```

# STYLE

- *#window*
- *window#waybar.empty #window* When no windows are in the workspace

The following classes are applied to the entire Waybar rather than just the
window widget:

- *window#waybar.empty* When no windows are in the workspace
- *window#waybar.solo* When one tiled window is visible in the workspace
  (floating windows may be present)
- *window#waybar.<app_id>* Where *<app_id>* is the *class* (e.g. *chromium*) of
  the solo tiled window in the workspace (use *hyprctl clients* to see classes)
- *window#waybar.floating* When there are only floating windows in the workspace
- *window#waybar.fullscreen* When there is a fullscreen window in the workspace;
  useful with Hyprland's *fullscreen, 1* mode
- *window#waybar.swallowing* When there is a swallowed window in the workspace
