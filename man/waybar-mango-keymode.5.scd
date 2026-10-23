waybar-mango-keymode(5)

# NAME

waybar - mango keymode module

# DESCRIPTION

The *keymode* module displays the current keyboard mode (e.g. "resize", "default") in the Mango compositor. It is hidden when no mode is active.

# CONFIGURATION

Addressed by *mango/keymode*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how the mode should be displayed. *{mode}* is replaced by the current mode name.

*format-<mode>*: ++
	typeof: string ++
	Provide a custom format for a specific keymode. *<mode>* is the mode name as reported by Mango (e.g. "resize"). The value can contain *{mode}* as a placeholder.
	If this option is set, it overrides the main *format* for that mode.

*menu*: ++
	typeof: string ++
	Action that pops up a menu.

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file.

*menu-actions*: ++
	typeof: array ++
	Actions for the menu buttons.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables the module to consume all leftover space.

# FORMAT REPLACEMENTS

*{mode}*: The name of the current keymode.

# EXAMPLES

```
"mango/keymode": {
	"format": "[{mode}]",
	"format-resize": " Resizing"
}
```

# STYLE

- *#keymode*

A CSS class with the current mode name (e.g. *.resize*) is added to the widget, allowing per‑mode styling:

```
#keymode.resize { background: #ff0000; }
```
