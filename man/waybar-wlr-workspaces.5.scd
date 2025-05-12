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
	Should workspaces be sorted by name. Workspace names will be sorted numerically when all names are numbers.

*sort-by-coordinates*: ++
	typeof: bool ++
	default: false ++
	Should workspaces be sorted by coordinates. ++
	Note that if both  *sort-by-name* and *sort-by-coordinates* are true sort-by name will be first. If both are false - sort by id will be performed.

*sort-by-id*: ++
	typeof: bool ++
	default: false ++
	Should workspaces be sorted by ID. Workspace ID will be sorted numerically when all ID are numbers. Takes precedence over any other sort-by option.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false workspaces group will be shown only in assigned output. Otherwise, all workspace groups are shown.

*active-only*: ++
	typeof: bool ++
	default: false ++
	If set to true only active or urgent workspaces will be shown.

# FORMAT REPLACEMENTS

*{name}*: Name of workspace assigned by compositor.

*{id}*: ID of workspace assigned by compositor.

*{icon}*: Icon, as defined in *format-icons*.

# CLICK ACTIONS

*activate*: Switch to workspace.

*close*: Close the workspace.

# ICONS

In addition to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string match is found.
- *active*: Will be shown, when workspace is active

# EXAMPLES

```
"wlr/workspaces": {
	"format": "{name}: {icon}",
	"on-click": "activate",
	"format-icons": {
		"Workspace 1": "",
		"Workspace 2": "",
		"Workspace 3": "",
		"Workspace 4": "",
		"active": "",
		"default": ""
	},
	"sort-by-id": true
}
```

# Style

- *#workspaces*
- *#workspaces button*
- *#workspaces button.active*
- *#workspaces button.urgent*
- *#workspaces button.hidden*
