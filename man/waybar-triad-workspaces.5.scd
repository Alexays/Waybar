waybar-triad-workspaces(5)

# NAME

waybar - Triad workspaces module

# DESCRIPTION

The *workspaces* module shows Triad workspaces. It talks to Triad through the
native IPC socket, so it uses Triad's stable tag IDs while displaying the
workspace index and name you see in the session.

# CONFIGURATION

Addressed by *triad/workspaces*

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If false, the bar shows only the workspaces on its own output. If true, each
	bar shows every workspace.

*format*: ++
	typeof: string ++
	default: {value} ++
	The text shown for each workspace.

*format-icons*: ++
	typeof: array ++
	Chooses an icon by workspace name, index, or state. See *icons*.

*disable-click*: ++
	typeof: bool ++
	default: false ++
	If false, clicking a workspace focuses it in Triad.

*disable-markup*: ++
	typeof: bool ++
	default: false ++
	If true, labels are shown as plain text instead of Pango markup.

*current-only*: ++
	typeof: bool ++
	default: false ++
	If true, show only the focused workspace, or only the visible workspace on
	this output when *all-outputs* is false.

*hide-empty*: ++
	typeof: bool ++
	default: false ++
	If true, hide empty workspaces unless they are focused.

*on-update*: ++
	typeof: string ++
	Command to run when the module updates.

*expand*: ++
	typeof: bool ++
	default: false ++
	Allows this module to take remaining bar space.

# FORMAT REPLACEMENTS

*{value}*: The workspace name, or its index when it has no name.

*{name}*: The workspace name.

*{icon}*: The selected icon.

*{index}*: The workspace index.

*{id}*: The stable Triad tag ID.

*{output}*: The output where the workspace is visible.

*{layout}*: The current layout ID for the workspace.

# ICONS

The module first checks the state icons below, then the workspace name, then the
workspace index.

- *default*: Used when nothing else matches.
- *focused*: The globally focused workspace.
- *active*: A workspace visible on an output.
- *urgent*: A workspace with urgent windows.
- *empty*: A workspace with no windows.

# EXAMPLES

```
"triad/workspaces": {
	"format": "{icon}",
	"format-icons": {
		"term": "",
		"web": "",
		"focused": "",
		"active": "",
		"default": ""
	}
}
```

# STYLE

- *#workspaces button*
- *#workspaces button.focused*: The globally focused workspace.
- *#workspaces button.active*: A workspace visible on an output.
- *#workspaces button.urgent*: A workspace with urgent windows.
- *#workspaces button.empty*: A workspace with no windows.
- *#workspaces button.current_output*: A workspace on the same output as the bar.
- *#workspaces button#triad-workspace-<name>*: A workspace with this name, or
  this index when unnamed.
