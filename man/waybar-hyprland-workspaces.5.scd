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
	Based on the workspace ID and state, the corresponding icon gets selected. See *icons*.

*window-rewrite*: ++
	typeof: object ++
	Regex rules to map window class to an icon or preferred method of representation for a workspace's window.
	Keys are the rules, while the values are the methods of representation. Values may use the placeholders {class} and {title} to use the window's original class and/or title respectively.
	Rules may specify `class<...>`, `title<...>`, or both in order to fine-tune the matching.
	You may assign an empty value to a rule to have it ignored from generating any representation in workspaces.

*window-rewrite-default*:
	typeof: string ++
	default: "?" ++
	The default method of representation for a workspace's window. This will be used for windows whose classes do not match any of the rules in *window-rewrite*.

*format-window-separator*: ++
	typeof: string ++
	default: " " ++
	The separator to be used between windows in a workspace.

*show-special*: ++
	typeof: bool ++
	default: false ++
	If set to true, special workspaces will be shown.

*special-visible-only*: ++
	typeof: bool ++
	default: false ++
	If this and show-special are to true, special workspaces will be shown only if visible.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false workspaces group will be shown only in assigned output. Otherwise, all workspace groups are shown.

*active-only*: ++
	typeof: bool ++
	default: false ++
	If set to true, only the active workspace will be shown.

*move-to-monitor*: ++
	typeof: bool ++
	default: false ++
	If set to true, open the workspace on the current monitor when clicking on a workspace button.
	Otherwise, the workspace will open on the monitor where it was previously assigned.
	Analog to using `focusworkspaceoncurrentmonitor` dispatcher instead of `workspace` in Hyprland.

*ignore-workspaces*: ++
	typeof: array ++
	default: [] ++
	Regexes to match against workspaces names. If there's a match, the workspace will not be shown. This takes precedence over *show-special*, *all-outputs*, and *active-only*.

*sort-by*: ++
	typeof: string ++
	default: "default" ++
	If set to number, workspaces will sort by number.
	If set to name, workspaces will sort by name.
	If set to id, workspaces will sort by id.
	If none of those, workspaces will sort with default behavior.

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

```
"hyprland/workspaces": {
	"format": "{name}\n{windows}",
	"format-window-separator": "\n",
	"window-rewrite-default": "",
	"window-rewrite": {
		"title<.*youtube.*>": "", // Windows whose titles contain "youtube"
		"class<firefox>": "", // Windows whose classes are "firefox"
		"class<firefox> title<.*github.*>": "", // Windows whose class is "firefox" and title contains "github". Note that "class" always comes first.
		"foot": "", // Windows that contain "foot" in either class or title. For optimization reasons, it will only match against a title if at least one other window explicitly matches against a title.
		"code": "󰨞",
		"title<.* - (.*) - VSCodium>": "codium $1"  // captures part of the window title and formats it into output
	}
}
```

```
"hyprland/workspaces": {
	// Formatting omitted for brevity
	"ignore-workspaces": [
		"(special:)?chrome-sharing-indicator"
	]
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
- *#workspaces button.hosting-monitor* (gets applied if workspace-monitor == waybar-monitor)
