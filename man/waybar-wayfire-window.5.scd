waybar-wayfire-window(5)

# NAME

waybar - wayfire window module

# DESCRIPTION

The *window* module displays the title of the currently focused window in wayfire.

# CONFIGURATION

Addressed by *wayfire/window*

*format*: ++
	typeof: string ++
	default: {title} ++
	The format, how information should be displayed. On {} the current window title is displayed.

*rewrite*: ++
	typeof: object ++
	Rules to rewrite window title. See *rewrite rules*.

*icon*: ++
	typeof: bool ++
	default: false ++
	Option to hide the application icon.

*icon-size*: ++
	typeof: integer ++
	default: 24 ++
	Option to change the size of the application icon.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# FORMAT REPLACEMENTS

See the output of "wayfire msg windows" for examples

*{title}*: The current title of the focused window.

*{app_id}*: The current app ID of the focused window.

# REWRITE RULES

*rewrite* is an object where keys are regular expressions and values are
rewrite rules if the expression matches. Rules may contain references to
captures of the expression.

Regular expression and replacement follow ECMA-script rules.

If no expression matches, the title is left unchanged.

Invalid expressions (e.g., mismatched parentheses) are skipped.

# EXAMPLES

```
"wayfire/window": {
	"format": "{}",
	"rewrite": {
		"(.*) - Mozilla Firefox": "🌎 $1",
		"(.*) - zsh": "> [$1]"
	}
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
