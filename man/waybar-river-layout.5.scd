waybar-river-layout(5)

# NAME

waybar - river layout module

# DESCRIPTION

The *layout* module displays the current layout in river.

It may not be set until a layout is first applied.

# CONFIGURATION

Addressed by *river/layout*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} data gets inserted.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*min-length*: ++
	typeof: integer ++
	The minimum length in characters the module should take up.

*align*: ++
	typeof: float ++
	The alignment of the text, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right clicked on the module.

# EXAMPLE

```
"river/layout": {
	"format": "{}",
	"min-length": 4,
	"align": "right"
}
```

# STYLE

- *#layout*
- *#layout.focused* Applied when the output this module's bar belongs to is focused.

# SEE ALSO

waybar(5), river(1)
