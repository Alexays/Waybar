waybar-niri-workspaces(5)

# NAME

waybar - niri workspaces module

# DESCRIPTION

The *workspaces* module displays the currently used workspaces in niri.

# CONFIGURATION

Addressed by *niri/workspaces*

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false, workspaces will only be shown on the output they are on. If set to true all workspaces will be shown on every output.

*sort-by-name*: ++
	typeof: bool ++
	default: false ++
	Sort workspaces by name (numeric sort when all names are numbers). Unnamed workspaces fall back to their index on the output.

*sort-by-coordinates*: ++
	typeof: bool ++
	default: false ++
	Sort workspaces by output and index. If both *sort-by-name* and *sort-by-coordinates* are true, sorting by name is applied.

*sort-by-id*: ++
	typeof: bool ++
	default: false ++
	Sort workspaces by id, taking precedence over other sort options.

If none of the sorting options are enabled, workspaces keep their output/index order.

*format*: ++
	typeof: string ++
	default: {value} ++
	The format, how information should be displayed.

*format-icons*: ++
	typeof: array ++
	Based on the workspace name, index and state, the corresponding icon gets selected. See *icons*.

*disable-click*: ++
	typeof: bool ++
	default: false ++
	If set to false, you can click to change workspace. If set to true this behaviour is disabled.

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

*{value}*: Name of the workspace, or index for unnamed workspaces,
as defined by niri.

*{name}*: Name of the workspace for named workspaces.

*{icon}*: Icon, as defined in *format-icons*.

*{index}*: Index of the workspace on its output.

*{output}*: Output where the workspace is located.

# ICONS

Additional to workspace name matching, the following *format-icons* can be set.

- *default*: Will be shown, when no string matches are found.
- *focused*: Will be shown, when workspace is focused.
- *active*: Will be shown, when workspace is active on its output.
- *urgent*: Will be shown, when workspace has urgent windows.
- *empty*: Will be shown, when workspace is empty.

# EXAMPLES

```
"niri/workspaces": {
	"format": "{icon}",
	"format-icons": {
		// Named workspaces
		// (you need to configure them in niri)
		"browser": "",
		"discord": "",
		"chat": "<b></b>",

		// Icons by state
		"active": "",
		"default": ""
	}
}
```

# Style

- *#workspaces button*
- *#workspaces button.focused*: The single focused workspace.
- *#workspaces button.active*: The workspace is active (visible) on its output.
- *#workspaces button.urgent*: The workspace has one or more urgent windows.
- *#workspaces button.empty*: The workspace is empty.
- *#workspaces button.current_output*: The workspace is from the same output as
  the bar that it is displayed on.
- *#workspaces button#niri-workspace-<name>*: Workspaces named this, or index
  for unnamed workspaces.
