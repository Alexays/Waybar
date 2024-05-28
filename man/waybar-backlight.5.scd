waybar-backlight(5)

# NAME

waybar - backlight module

# DESCRIPTION

The *backlight* module displays the current backlight level.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	default: 2 ++
	The interval in which information gets polled.

*format*: ++
	typeof: string ++
	default: {percent}% ++
	The format, how information should be displayed. On {} data gets inserted.

*max-length*: ++
	typeof: integer ++
	The maximum length in characters the module should display.

*min-length*: ++
	typeof: integer ++
	The minimum length in characters the module should accept.

*align*: ++
	typeof: float ++
	The alignment of the label within the module, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*justify*: ++
	typeof: string ++
	The alignment of the text within the module's label, allowing options 'left', 'right', or 'center' to define the positioning.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*states*: ++
	typeof: object ++
	A number of backlight states which get activated on certain brightness levels. See *waybar-states(5)*.

*on-click*: ++
	typeof: string ++
	Command to execute when the module is clicked.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mouse scroll wheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when the module is right-clicked.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when performing a scroll up on the module. This replaces the default behaviour of brightness control.

*on-scroll-down*: ++
	typeof: string
	Command to execute when performing a scroll down on the module. This replaces the default behaviour of brightness control.

*smooth-scrolling-threshold*: ++
	typeof: double
	Threshold to be used when scrolling.

*reverse-scrolling*: ++
	typeof: bool ++
	Option to reverse the scroll direction.

*scroll-step*: ++
	typeof: float ++
	default: 1.0 ++
	The speed at which to change the brightness when scrolling.

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

# EXAMPLE:

```
"backlight": {
	"device": "intel_backlight",
	"format": "{percent}% {icon}",
	"format-icons": ["", ""]
}
```

# STYLE

- *#backlight*
