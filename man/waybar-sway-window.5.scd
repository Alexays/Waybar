waybar-sway-window(5)

# NAME

waybar - sway window module

# DESCRIPTION

The *window* module displays the title of the currently focused window in Sway

# CONFIGURATION

Addressed by *sway/window*

*format*: ++
	typeof: string ++
	default: {title} ++
	The format, how information should be displayed.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*min-length*: ++
	typeof: integer ++
	The minimum length in characters the module should take up.

*align*: ++
	typeof: float ++
	The alignment of the text, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right clicked on the module.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when scrolling up on the module.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	Option to show the focused window along with its workspace styles on all outputs.

*offscreen-css*: ++
	typeof: bool ++
	default: false ++
	Only effective when all-outputs is true. Adds style according to present windows on unfocused outputs instead of showing the focused window and style.

*offscreen-css-text*: ++
	typeof: string ++
	Only effective when both all-outputs and offscreen-style are true. On screens currently not focused, show the given text along with that workspaces styles.

*show-focused-workspace-name*: ++
	typeof: bool ++
	default: false ++
	If the workspace itself is focused and the workspace contains nodes or floating_nodes, show the workspace name. If not set, text remains empty but styles according to nodes in the workspace are still applied.

*rewrite*: ++
	typeof: object ++
	Rules to rewrite the module format output. See *rewrite rules*.

*icon*: ++
	typeof: bool ++
	default: false ++
	Option to hide the application icon.

*icon-size*: ++
	typeof: integer ++
	default: 24 ++
	Option to change the size of the application icon.

# FORMAT REPLACEMENTS

*{title}*: The title of the focused window.

*{app_id}*: The app_id of the focused window.

*{shell}*: The shell of the focused window. It's 'xwayland' when the window is
running through xwayland, otherwise it's 'xdg-shell'.

# REWRITE RULES

*rewrite* is an object where keys are regular expressions and values are
rewrite rules if the expression matches. Rules may contain references to
captures of the expression.

Regular expression and replacement follow ECMA-script rules.

If no expression matches, the format output is left unchanged.

Invalid expressions (e.g., mismatched parentheses) are skipped.

# EXAMPLES

```
"sway/window": {
	"format": "{}",
	"max-length": 50,
	"rewrite": {
		"(.*) - Mozilla Firefox": "🌎 $1",
		"(.*) - zsh": "> [$1]"
	}
}
```

# STYLE

- *#window*

The following classes are applied to the entire Waybar rather than just the
window widget:

- *window#waybar.empty* When no windows are in the workspace, or screen is not focused and offscreen-css option is not set
- *window#waybar.solo* When one tiled window is in the workspace
- *window#waybar.floating* When there are only floating windows in the workspace
- *window#waybar.stacked* When there is more than one window in the workspace and the workspace layout is stacked
- *window#waybar.tabbed* When there is more than one window in the workspace and the workspace layout is tabbed
- *window#waybar.tiled* When there is more than one window in the workspace and the workspace layout is splith or splitv
- *window#waybar.<app_id>* Where *app_id* is the app_id or *instance* name like (*chromium*) of the only window in the workspace
