waybar-triad-window(5)

# NAME

waybar - Triad window module

# DESCRIPTION

The *window* module shows the title of the focused Triad window. By default it
follows the globally focused workspace. With *separate-outputs*, it follows the
workspace visible on the bar's output.

# CONFIGURATION

Addressed by *triad/window*

*format*: ++
	typeof: string ++
	default: {title} ++
	The text to display. A bare {} displays the current title.

*rewrite*: ++
	typeof: object ++
	Rules for rewriting window titles. See *rewrite rules*.

*separate-outputs*: ++
	typeof: bool ++
	Show the focused window for the output the bar belongs to, instead of the
	global focused window.

*icon*: ++
	typeof: bool ++
	default: false ++
	Shows or hides the application icon.

*icon-size*: ++
	typeof: integer ++
	default: 24 ++
	Sets the application icon size.

*expand*: ++
	typeof: bool ++
	default: false ++
	Allows this module to take remaining bar space.

# FORMAT REPLACEMENTS

Use `triad msg windows` to inspect the fields Triad reports.

*{title}*: The title of the focused window.

*{app_id}*: The app ID of the focused window.

# REWRITE RULES

*rewrite* is an object. Keys are regular expressions. Values are replacements
used when a key matches the title, and may refer to capture groups.

Regular expressions and replacements follow ECMAScript rules. If no expression
matches, the title is shown as Triad reported it. Invalid expressions are
skipped.

# EXAMPLES

```
"triad/window": {
	"format": "{title}",
	"rewrite": {
		"(.*) - Mozilla Firefox": "$1",
		"(.*) - zsh": "> [$1]"
	}
}
```

# STYLE

- *#window*
- *window#waybar.empty #window*: No window is focused for the selected workspace.

The classes below are applied to the Waybar window:

- *window#waybar.empty*: No window is focused for the selected workspace.
- *window#waybar.solo*: Only one window is on the selected workspace.
- *window#waybar.<app-id>*: The app ID of that single window.
