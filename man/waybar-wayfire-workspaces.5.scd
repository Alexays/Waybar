waybar-wayfire-workspaces(5)

# NAME

waybar - wayfire workspaces module

# DESCRIPTION

The *workspaces* module displays the currently used workspaces in wayfire.

# CONFIGURATION

Addressed by *wayfire/workspaces*

*format*: ++
	typeof: string ++
	default: {index} ++
	The format, how information should be displayed.

*format-icons*: ++
	typeof: object ++
	Based on the workspace name, index and state, the corresponding icon gets selected. See *icons*.

*disable-click*: ++
	typeof: bool ++
	default: false ++
	If set to false, you can click to change workspace. If set to true this behaviour is disabled.

*disable-scroll*: ++
	typeof: bool ++
	default: false ++
	If set to false, you can scroll to cycle through workspaces. If set to true this behaviour is disabled.

*enable-bar-scroll*: ++
	typeof: bool ++
	default: false ++
	If set to false, you can't scroll to cycle through workspaces from the entire bar. If set to true this behaviour is enabled.

*disable-markup*: ++
	typeof: bool ++
	default: false ++
	If set to true, button label will escape pango markup.

*current-only*: ++
	typeof: bool ++
	default: false ++
	If set to true, only the active or focused workspace will be shown.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# FORMAT REPLACEMENTS

*{icon}*: Icon, as defined in *format-icons*.

*{index}*: Index of the workspace on its output.

*{output}*: Output where the workspace is located.

# ICONS

Additional to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string matches are found.
- *focused*: Will be shown, when workspace is focused.

# EXAMPLES

```
"wayfire/workspaces": {
	"format": "{icon}",
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
- *#workspaces button.focused*: The single focused workspace.
- *#workspaces button.empty*: The workspace is empty.
- *#workspaces button.current_output*: The workspace is from the same output as
  the bar that it is displayed on.
