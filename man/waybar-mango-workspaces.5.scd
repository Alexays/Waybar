waybar-mango-workspaces(5)

# NAME

waybar - mango workspaces module

# DESCRIPTION

The *workspaces* module displays the tags (workspaces) of the Mango compositor. It shows an overview button when the overview mode is active (active tag is 0), and individual tag buttons otherwise.

# CONFIGURATION

Addressed by *mango/workspaces*

*format*: ++
	typeof: string ++
	default: {value} ++
	The format for each tag button. See *FORMAT REPLACEMENTS*.

*format-icons*: ++
	typeof: object ++
	Icons to be used instead of the workspace index or name. Keys can be a workspace index (as a string), or one of the following special state keys: *default*, *active*, *urgent*, *empty*.

*disable-markup*: ++
	typeof: bool ++
	default: false ++
	If true, the button label will not be interpreted as Pango markup.

*current-only*: ++
	typeof: bool ++
	default: false ++
	If true, only the currently active workspace button is shown.

*hide-empty*: ++
	typeof: bool ++
	default: false ++
	If true, buttons for empty (client_count == 0) workspaces are hidden, unless the workspace is active.

*on-click*: ++
	typeof: string ++
	Command to execute on left click. Typically set to *activate* or *toggle*.

*on-click-middle*: ++
	typeof: string ++
	Command for middle click. Same actions as *on-click*.

*on-click-right*: ++
	typeof: string ++
	Command for right click. Same actions as *on-click*.

*overview-label*: ++
	typeof: string ++
	default: "OVERVIEW" ++
	Label shown on the overview button when the overview is active.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables the module to consume leftover space.

# FORMAT REPLACEMENTS

*{value}*: Workspace index (same as *{index}* for unnamed workspaces).

*{name}*: Workspace name (equal to the index for unnamed workspaces in Mango).

*{icon}*: Icon selected from *format-icons* based on workspace index and state.

*{index}*: Numeric index of the workspace.

*{output}*: Name of the output where the workspace is located.

# CLICK ACTIONS

When a tag button is clicked, the action from *on-click* (or its middle/right variants) is evaluated.
Supported actions:

- *activate*: dispatch view,<index>
- *toggle*: dispatch toggleview,<index>

When the overview button is clicked, the actions change to:

- *activate*: dispatch overview
- *toggle*: dispatch toggleoverview

# EXAMPLES

```
"mango/workspaces": {
	"format": "{icon}",
	"format-icons": {
		"1": "一",
		"2": "二",
		"active": "",
		"default": "",
		"urgent": "",
		"empty": ""
	},
	"on-click": "activate",
	"on-click-right": "toggle",
	"overview-label": ""
}
```

# STYLE

- *#workspaces button*
- *#workspaces button.active* – the workspace is active (visible) on its output.
- *#workspaces button.urgent* – the workspace has at least one urgent window.
- *#workspaces button.empty* – the workspace contains no clients.
- *#workspaces button.current_output* – the workspace belongs to the output where the bar is shown.
- *#workspaces button.overview* – the overview button (visible in overview mode).