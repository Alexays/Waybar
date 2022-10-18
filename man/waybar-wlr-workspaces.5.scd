waybar-wlr-workspaces(5)

# NAME

waybar - wlr workspaces module

# DESCRIPTION

The *workspaces* module displays the currently used workspaces in wayland compositor.

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
	default: true ++
	Should workspaces be sorted by coordinates.
	Note that if both  *sort-by-name* and *sort-by-coordinates* are true sort by name will be first.
	If both are false - sort by id will be performed.

*sort-by-number*: ++
	typeof: bool ++
	default: false ++
	If set to true, workspace names will be sorted numerically. Takes presedence over any other sort-by option.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false workspaces group will be shown only in assigned output. Otherwise all workspace groups are shown.

*active-only*: ++
	typeof: bool ++
	default: false ++
	If set to true only active or urgent workspaces will be shown.

# FORMAT REPLACEMENTS

*{name}*: Name of workspace assigned by compositor

*{icon}*: Icon, as defined in *format-icons*.

# CLICK ACTIONS

*activate*: Switch to workspace.

*close*: Close the workspace.

# ICONS

Additional to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string match is found.
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
	},
	"sort-by-number": true
}
```

# Style

- *#workspaces*
- *#workspaces button*
- *#workspaces button.active*
- *#workspaces button.urgent*
- *#workspaces button.hidden*
