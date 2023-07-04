waybar-wlr-workspaces(5)

# NAME

waybar - hyprland workspaces module

# DESCRIPTION

The *workspaces* module displays the currently used workspaces in hyprland compositor.

# CONFIGURATION

Addressed by *hyprland/workspaces*

*format*: ++
	typeof: string ++
	default: {id} ++
	The format, how information should be displayed.

*format-icons*: ++
	typeof: array ++
	Based on the workspace id and state, the corresponding icon gets selected. See *icons*.

# FORMAT REPLACEMENTS

*{id}*: id of workspace assigned by compositor

*{icon}*: Icon, as defined in *format-icons*.

# ICONS

Additional to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string match is found.
- *active*: Will be shown, when workspace is active

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
		"active": "",
		"default": ""
	},
	"sort-by-number": true
}
```

# Style

- *#workspaces*
- *#workspaces button*
- *#workspaces button.active*
