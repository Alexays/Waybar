waybar-sway-taskbar(5)

# NAME

waybar - sway taskbar module

# DESCRIPTION

The *taskbar* module displays the currently open applications. This version
sources its data from the sway IPC protocol and requires running under sway.

# CONFIGURATION

Addressed by *sway/taskbar*

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false, applications on the bar's current output and current
	workspace are shown. Otherwise, all applications on all outputs and
	workspaces are shown.

*bar-css-states*: ++
	typeof: bool ++
	default: false ++
	If set to true, application state is exposed as CSS classes on the Waybar
	window, aggregated over the current workspace. Derived from the sway IPC
	tree. See *Bar state style* below.

*format*: ++
	typeof: string ++
	default: {icon} ++
	The format, how information should be displayed.

*icon-theme*: ++
	typeof: array|string ++
	The names of the icon-themes that should be used to find an icon. The list will be traversed from left to right. If omitted, the system default will be used.

*icon-size*: ++
	typeof: integer ++
	default: 16 ++
	The size of the icon.

*markup*: ++
	typeof: bool ++
	default: false ++
	If set to true, pango markup will be accepted in format and tooltip-format.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	If set to false no tooltip will be shown.

*tooltip-format*: ++
	typeof: string ++
	default: {title} ++
	The format, how information in the tooltip should be displayed.

*active-first*: ++
	typeof: bool ++
	default: false ++
	If set to true, always reorder the tasks in the taskbar so that the
	currently active one is first. Otherwise don't reorder. Overridden once the
	tasks have been reordered by dragging; see *REORDERING*.

*active-only*: ++
	typeof: bool ++
	default: false ++
	If set to true, only the currently active application button is shown.
	Other applications remain tracked and reappear when activated.

*sort-by-app-id*: ++
	typeof: bool ++
	default: false ++
	If set to true, group tasks by their app_id. May be combined with
	'active-first', in which case tasks are grouped by app_id first and the
	active task is then moved to the front. Both are overridden once the tasks
	have been reordered by dragging; see *REORDERING*.

*homogeneous*: ++
	typeof: bool ++
	default: false ++
	If set to true, distribute every task button evenly across the taskbar's allocated width. Buttons will automatically resize so that 'N' visible tasks each take '1/N' of the width.

*justify*: ++
	typeof: string ++
	The alignment of the text within the module's box, allowing options 'left', 'right', or 'center' to define the positioning.

*expand*: ++
	typeof: bool ++
	default: false ++
	If set to true, task buttons stretch to fill the available space in the taskbar and long titles are ellipsized to fit. Only takes effect on a horizontal bar; on a vertical bar the buttons keep their content-based size. If set to false, buttons are sized to their content.

*truncate*: ++
	typeof: bool ++
	default: false ++
	If set to true, the task button text will be ellipsized (truncated with …) when the available button width is smaller than the label text.

*on-click*: ++
	typeof: string ++
	The action which should be triggered when clicking on the application button with the left mouse button.

*on-click-middle*: ++
	typeof: string ++
	The action which should be triggered when clicking on the application button with the middle mouse button.

*on-click-right*: ++
	typeof: string ++
	The action which should be triggered when clicking on the application button with the right mouse button.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*ignore-list*: ++
	typeof: array ++
	List of app_id/titles to be invisible.

*squash-list*: ++
	typeof: array ++
	List of app_id/titles whose multiple instances are collapsed into a single button. When more than one instance of a listed app is open, only one button is shown; when one instance closes, the next hidden instance reappears. The special value '\*' matches all applications.

*app_ids-mapping*: ++
	typeof: object ++
	Dictionary of app_id to be replaced with

*rewrite*: ++
	typeof: object ++
	Rules to rewrite the module format output. See *rewrite rules*.

# FORMAT REPLACEMENTS

*{icon}*: The icon of the application.

*{name}*: The application name as in desktop file if appropriate desktop files are found, otherwise same as {app_id}

*{title}*: The title of the application.

*{app_id}*: The app_id (== application name) of the application.

*{state}*: The state (active, fullscreen, urgent) of the application.

*{short_state}*: The state (active == A, fullscreen == F, urgent == U) represented as one character of the application.

As with all format replacements in Waybar, a length limit such as *{title:.15}* is
measured in bytes, not characters. This may produce invalid text if a multi-byte
character is split, or (when *markup* is true) if the truncation cuts through a
value that must be escaped in XML.

# CLICK ACTIONS

*activate*: Bring the application into foreground.

*fullscreen*: Toggle application's fullscreen state.

*close*: Close the application.

# REORDERING

Task buttons can be reordered by dragging them onto each other with the left
mouse button. The dragged button is placed at the position of the button it is
dropped on.

The order produced by dragging takes precedence over *sort-by-app-id* and
*active-first*: from the first drag until Waybar is restarted, those options no
longer move the tasks they apply to. Newly opened windows are appended after
the tasks whose position was recorded by the drag.

The order is kept in memory only and is not persisted across restarts.

# REWRITE RULES

*rewrite* is an object where keys are regular expressions and values are
rewrite rules if the expression matches. Rules may contain references to
captures of the expression.

Regular expression and replacement follow ECMA-script rules.

An expression must match the format output *fully* to trigger its replacement.

If no expression matches, the format output is left unchanged.

Invalid expressions (e.g., mismatched parentheses) are skipped.

# EXAMPLES

```
"sway/taskbar": {
	"format": "{icon}",
	"icon-size": 14,
	"icon-theme": "Numix-Circle",
	"tooltip-format": "{title}",
	"on-click": "activate",
	"on-click-middle": "close",
	"ignore-list": [
	    "Alacritty"
	],
	"app_ids-mapping": {
		"firefoxdeveloperedition": "firefox-developer-edition"
	},
	"rewrite": {
		"Firefox Web Browser": "Firefox",
		"Foot Server": "Terminal"
	}
}
```

# Style

- *#taskbar*
- *#taskbar.empty*
- *#taskbar button*
- *#taskbar button.active*
- *#taskbar button.fullscreen*
- *#taskbar button.urgent*
- *#taskbar button.visible*

The *visible* class is set when the window's workspace is the one currently
displayed on its output. With *all-outputs* set to false every listed window is
on the current workspace, so the class is always present; it is chiefly useful
together with *all-outputs* to de-emphasise windows you are not looking at:

```
#taskbar button:not(.visible) {
	opacity: 0.5;
}
```

# Bar state style

When *bar-css-states* is enabled, the following classes are added to
*window#waybar*:

- *window#waybar.toplevel-active*
- *window#waybar.toplevel-fullscreen*
- *window#waybar.toplevel-urgent*

The active class describes the focused application. The fullscreen and
urgent classes are set if any application on the current workspace has that
state.

For example:

```
window#waybar {
	background-color: rgba(0, 0, 0, 0.5);
}

window#waybar.toplevel-urgent {
	background-color: rgba(64, 0, 0, 0.75);
}
```
