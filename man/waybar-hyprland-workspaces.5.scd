waybar-hyprland-workspaces(5)

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

*show-special*: ++
	typeof: bool ++
	default: false ++
	If set to true, special workspaces will be shown.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false workspaces group will be shown only in assigned output. Otherwise all workspace groups are shown.

*active-only*: ++
	typeof: bool ++
	default: false ++
	If set to true, only the active workspace will be shown.

# FORMAT REPLACEMENTS

*{id}*: id of workspace assigned by compositor

*{name}*: workspace name assigned by compositor

*{icon}*: Icon, as defined in *format-icons*.

# ICONS

Additional to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string match is found and none of the below conditions have defined icons.
- *active*: Will be shown, when workspace is active
- *special*: Will be shown on non-active special workspaces
- *empty*: Will be shown on non-active, non-special empty persistent workspaces
- *visible*: Will be shown on workspaces that are visible but not active. For example: this is useful if you want your visible workspaces on other monitors to have the same look as active.
- *persistent*: Will be shown on non-empty persistent workspaces

# EXAMPLES

```
"hyprland/workspaces": {
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
	"persistent-workspaces": {
		"*": 5, // 5 workspaces by default on every monitor
		"HDMI-A-1": 3 // but only three on HDMI-A-1
	}
}
```

```
"hyprland/workspaces": {
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
	"persistent-workspaces": {
		"*": [ 2,3,4,5 ], // 2-5 on every monitor
		"HDMI-A-1": [ 1 ] // but only workspace 1 on HDMI-A-1
	}
}
```

# Style

- *#workspaces*
- *#workspaces button*
- *#workspaces button.active*
- *#workspaces button.empty*
- *#workspaces button.visible*
- *#workspaces button.persistent*
- *#workspaces button.special*
- *#workspaces button.urgent*
