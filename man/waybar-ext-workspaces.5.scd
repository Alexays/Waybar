waybar-wlr-workspaces(5)

# NAME

waybar - wlr workspaces module

# DESCRIPTION

The *workspaces* module displays the currently used workspaces in wayland compositor.

To use this module, your compositor must implement the *ext-workspace-v1*
Wayland protocol.

# CONFIGURATION

Addressed by *ext/workspaces*

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

*sort-by-number*: ++
	typeof: bool ++
	Deprecated alias of *sort-by-id*. Setting it emits a warning; prefer *sort-by-id*.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false workspaces group will be shown only in assigned output. Otherwise, all workspace groups are shown.

*active-only*: ++
	typeof: bool ++
	default: false ++
	If set to true only active or urgent workspaces will be shown.

*ignore-hidden*: ++
	typeof: bool ++
	default: true ++
	If set to false hidden workspaces will be shown.

*on-click*: ++
	typeof: string ++
	The action to perform on left-click. See *CLICK ACTIONS* for the possible values.

*on-click-middle*: ++
	typeof: string ++
	The action to perform on middle-click. See *CLICK ACTIONS* for the possible values.

*on-click-right*: ++
	typeof: string ++
	The action to perform on right-click. See *CLICK ACTIONS* for the possible values.

# FORMAT REPLACEMENTS

*{name}*: Name of workspace assigned by compositor.

*{id}*: ID of workspace assigned by compositor.

*{icon}*: Icon, as defined in *format-icons*.

# CLICK ACTIONS

*activate*: Switch to workspace.

*deactivate*: Deactivate the workspace.

*close*: Close the workspace.

# ICONS

In addition to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string match is found.
- *active*: Will be shown, when workspace is active

# EXAMPLES

```
"ext/workspaces": {
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
