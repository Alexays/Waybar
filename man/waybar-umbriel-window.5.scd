waybar-umbriel-window(5)

# NAME

waybar - umbriel window module

# DESCRIPTION

The *window* module displays the title of the currently focused window in Umbriel.

# CONFIGURATION

Addressed by *umbriel/window*

*format*: ++
	typeof: string ++
	default: {title} ++
	The format, how information should be displayed. See *FORMAT REPLACEMENTS*.

*rewrite*: ++
	typeof: object ++
	Rules to rewrite the window title. See *REWRITE RULES*.

*separate-outputs*: ++
	typeof: bool ++
	default: false ++
	Show the last focused window of the bar's output instead of the globally focused window.

*icon*: ++
	typeof: bool ++
	default: false ++
	Show the application icon.

*icon-size*: ++
	typeof: integer ++
	default: 24 ++
	Size of the application icon.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

*max-length*: ++
	typeof: integer ++
	The maximum length in characters the module should display.

*show-empty*: ++
	typeof: bool ++
	default: false ++
	When no window is focused (empty workspace or all windows closed), keep the module visible and render *format* with empty replacements and *{column}*, *{row}* and *{max_col}* set to -1. When false, the module is hidden while no window is focused.

# FORMAT REPLACEMENTS

*{title}*: The current title of the focused window.

*{app_id}*: The current app ID of the focused window.

*{id}*: The stable ID of the focused window.

*{workspace}*: The name of the workspace containing the focused window.

*{workspace_id}*: The stable ID of the workspace containing the focused window.

*{layout}*: The layout of the workspace containing the focused window.

*{output}*: The output containing the focused window.

*{column}*: The window's column in the scrolling layout, or -1 when it is floating or the layout has no columns.

*{row}*: The window's row in the scrolling layout, or -1 when it is floating or the layout has no columns.

*{max_col}*: The highest column index among the windows on the same workspace.

# REWRITE RULES

*rewrite* is an object where keys are regular expressions and values are
rewrite rules if the expression matches. Rules may contain references to
captures of the expression.

Regular expression and replacement follow ECMA-script rules.

If no expression matches, the title is left unchanged.

Invalid expressions (e.g., mismatched parentheses) are skipped.

# EXAMPLES

```
"umbriel/window": {
	"format": "{}",
	"rewrite": {
		"(.*) - Mozilla Firefox": "🌎 $1",
		"(.*) - zsh": "> [$1]"
	},
	"icon": true,
	"icon-size": 20
}
```

# STYLE

- *#window*
- *window#waybar.empty #window* When no windows are on the workspace

The following classes are applied to the entire Waybar rather than just the
window widget:

- *window#waybar.empty* When no windows are in the workspace
- *window#waybar.solo* When only one window is on the workspace
- *window#waybar.<app-id>* Where *app-id* is the app ID of the only window on
  the workspace
