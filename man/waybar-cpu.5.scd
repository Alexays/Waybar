waybar-cpu(5)

# NAME

waybar - cpu module

# DESCRIPTION

The *cpu* module displays the current cpu utilization.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	default: 10 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string  ++
	default: {usage}% ++
	The format, how information should be displayed. On {} data gets inserted.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*min-length*: ++
    typeof: integer ++
    The minimum length in characters the module should take up.

*align*: ++
    typeof: float ++
    The alignment of the text, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*states*: ++
	typeof: array ++
	A number of cpu usage states which get activated on certain usage levels. See *waybar-states(5)*.

*on-click*: ++
	typeof: string  ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right clicked on the module.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when scrolling up on the module.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{load}*: Current cpu load.

*{usage}*: Current cpu usage.

# EXAMPLE

```
"cpu": {
	"interval": 10,
	"format": "{}% ",
	"max-length": 10
}
```

# STYLE

- *#cpu*
