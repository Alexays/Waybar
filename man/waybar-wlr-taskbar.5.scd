waybar-wlr-taskbar(5)

# NAME

wlroots - Taskbar module

# DESCRIPTION

The *taskbar* module displays the currently open applications. This module requires
a compositor that implements the foreign-toplevel-manager interface.

# CONFIGURATION

Addressed by *wlr/taskbar*

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false applications on the waybar's current output will be shown. Otherwise all applications are shown.

*format*: ++
	typeof: string ++
	default: {icon} ++
	The format, how information should be displayed.

*icon-theme*: ++
	typeof: string ++
	The name of the icon-theme that should be used. If omitted, the system default will be used.

*icon-size*: ++
	typeof: integer ++
	default: 16 ++
	The size of the icon.

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
	If set to true, always reorder the tasks in the taskbar so that the currently active one is first. Otherwise don't reorder.

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

# FORMAT REPLACEMENTS

*{icon}*: The icon of the application.

*{title}*: The title of the application.

*{app_id}*: The app_id (== application name) of the application.

*{state}*: The state (minimized, maximized, active, fullscreen) of the application.

*{short_state}*: The state (minimize == m, maximized == M, active == A, fullscreen == F) represented as one character of the application.

# CLICK ACTIONS

*activate*: Bring the application into foreground.
*minimize*: Minimize the application.
*maximize*: Maximize the application.
*fullscreen*: Set the application to fullscreen.
*close*: Close the application.

# EXAMPLES

```
"wlr/taskbar": {
	"format": "{icon}",
	"icon-size": 14,
	"icon-theme": "Numix-Circle",
	"tooltip-format": "{title}",
	"on-click": "activate",
	"on-click-middle": "close"
}
```

# Style

- *#taskbar*
- *#taskbar button*
- *#taskbar button.maximized*
- *#taskbar button.minimized*
- *#taskbar button.active*
- *#taskbar button.fullscreen*
