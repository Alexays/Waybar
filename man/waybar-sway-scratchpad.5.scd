waybar-sway-scratchpad(5)

# NAME

waybar - sway scratchpad module

# DESCRIPTION

The *scratchpad* module displays the scratchpad status in Sway

# CONFIGURATION

Addressed by *sway/scratchpad*

*format*: ++
	typeof: string ++
	default: {icon} {count} ++
	The format, how information should be displayed.

*show-empty*: ++
	typeof: bool ++
	default: false ++
	Option to show module when scratchpad is empty.

*format-icons*: ++
	typeof: array/object ++
	Based on the current scratchpad window counts, the corresponding icon gets selected.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	default: {app}: {title} ++
	The format, how information in the tooltip should be displayed.

*menu*: ++
	typeof: string ++
	Action that popups the menu.

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file. There need to be an element of type
	GtkMenu with id *menu*

*menu-actions*: ++
	typeof: array ++
	The actions corresponding to the buttons of the menu.

# FORMAT REPLACEMENTS

*{icon}*: Icon, as defined in *format-icons*.

*{count}*: Number of windows in the scratchpad.

*{app}*: Name of the application in the scratchpad.

*{title}*: Title of the application in the scratchpad.

# EXAMPLES

```
"sway/scratchpad": {
	"format": "{icon} {count}",
	"show-empty": false,
	"format-icons": ["", ""],
	"tooltip": true,
	"tooltip-format": "{app}: {title}"
}
```

# STYLE

- *#scratchpad*
- *#scratchpad.empty*
