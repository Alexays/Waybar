waybar-wlr-taskbar(5)

# NAME

waybar - wlr taskbar module

# DESCRIPTION

The *taskbar* module displays the currently open applications. This module requires
a compositor that implements the foreign-toplevel-manager interface.

# CONFIGURATION

Addressed by *wlr/taskbar*

*actions*: ++
    typeof: object ++
    Actions that can be triggered by clicking on the task button. Valid triggers within actions are *on-click*, *on-click-middle* and *on-click-right*. These can contain *click actions* as well as commands. The commands can have format replacements, see *format replacements*.

*all-outputs*: ++
	typeof: bool ++
	default: false ++
	If set to false applications on the waybar's current output will be shown. Otherwise, all applications are shown.

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
	If set to true, always reorder the tasks in the taskbar so that the currently active one is first. Otherwise don't reorder.

*sort-by-app-id*: ++
	typeof: bool ++
	default: false ++
	If set to true, group tasks by their app_id. Cannot be used with 'active-first'.

*on-click*: ++
	typeof: string ++
	May be defined on the module rather than in the actions object. If it contains a command, it is executed by AModule on the whole taskbar rather than individual tasks, and regardless of whether the same trigger is present in the actions object. If it contains an action (deprecated), the action is executed on individual tasks but if the same trigger was already defined in an actions object, the action from the actions object has preference.

*on-click-middle*: ++
	typeof: string ++
	See *on-click*.

*on-click-right*: ++
	typeof: string ++
	See *on-click*.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*ignore-list*: ++
	typeof: array ++
	List of app_id/titles to be invisible.

*app_ids-mapping*: ++
	typeof: object ++
	Dictionary of app_id to be replaced with

*rewrite*: ++
	typeof: object ++
	Rules to rewrite the module format output. See *rewrite rules*.

# FORMAT REPLACEMENTS

*{icon}*: The icon of the application. Not available for actions command.

*{name}*: The application name as in desktop file if appropriate desktop files are found, otherwise same as {app_id}. Not available for actions command.

*{title}*: The title of the application.

*{app_id}*: The app_id (== application name) of the application.

*{state}*: The state (minimized, maximized, active, fullscreen) of the application.

*{short_state}*: The state (minimize == m, maximized == M, active == A, fullscreen == F) represented as one character of the application. Not available for actions command.

# CLICK ACTIONS

*activate*: Bring the application into foreground.

*minimize*: Toggle application's minimized state.

*minimize-raise*: Bring the application into foreground or toggle its minimized state.

*maximize*: Toggle application's maximized state.

*fullscreen*: Toggle application's fullscreen state.

*close*: Close the application.

# REWRITE RULES

*rewrite* is an object where keys are regular expressions and values are
rewrite rules if the expression matches. Rules may contain references to
captures of the expression.

Regular expression and replacement follow ECMA-script rules.

If no expression matches, the format output is left unchanged.

Invalid expressions (e.g., mismatched parentheses) are skipped.

# EXAMPLES

```
"wlr/taskbar": {
	"format": "{icon}",
	"icon-size": 14,
	"icon-theme": "Numix-Circle",
	"tooltip-format": "{title}",
	"actions" : {
		"on-click": "activate",
		"on-click-middle": "close",
		"on-click-right": "xmessage {title} {app_id} {state}"
    },
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
- *#taskbar button*
- *#taskbar button.maximized*
- *#taskbar button.minimized*
- *#taskbar button.active*
- *#taskbar button.fullscreen*
