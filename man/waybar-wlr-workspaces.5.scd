waybar-wlr-workspaces(5)

# NAME

waybar - wlr workspaces module

# DESCRIPTION

The *workspaces* module displays the currently used workspaces in waynland compositor.

# CONFIGURATION

Addressed by *wlr/workspaces*

*format*: ++
	typeof: string ++
	default: {name} ++
	The format, how information should be displayed.

*format-icons*: ++
	typeof: array ++
	Based on the workspace name and state, the corresponding icon gets selected. See *icons*.

*sort-by-name*: ++
	typeof: bool ++
	default: true ++
	Should workspaces be sorted by name.

*sort-by-coordinates*: ++
	typeof: bool ++
	default: false ++
	Should workspaces be sorted by coordinates.
	Note that if both  *sort-by-name* and *sort-by-coordinates* are true sort by name will be first.
	If both are false - sort by id will be performed.

# FORMAT REPLACEMENTS

*{name}*: Number stripped from workspace value.

*{icon}*: Icon, as defined in *format-icons*.

# ICONS

Additional to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string matches is found.
- *focused*: Will be shown, when workspace is focused

# EXAMPLES

```
"wlr/workspaces": {
	"format": "{name}: {icon}",
	"format-icons": {
		"1": "",
		"2": "",
		"3": "",
		"4": "",
		"5": "",
		"focused": "",
		"default": ""
	}
}
```

# Style

- *#workspaces button*
- *#workspaces button.focused*
