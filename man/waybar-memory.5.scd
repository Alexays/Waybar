waybar-memory(5)

# NAME

waybar - memory module

# DESCRIPTION

The *memory* module displays the current memory utilization.

# CONFIGURATION

Addressed by *memory*

*interval*: ++
	typeof: integer++
	default: 30 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string ++
	default: {percentage}% ++
	The format, how information should be displayed.

*format-icons*: ++
	typeof: array/object ++
	Based on the current percentage, the corresponding icon gets selected. ++
	The order is *low* to *high*. Or by the state if it is an object.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*states*: ++
	typeof: object ++
	A number of memory utilization states which get activated on certain percentage thresholds. See *waybar-states(5)*.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*min-length*: ++
	typeof: integer ++
	The minimum length in characters the module should accept.

*align*: ++
	typeof: float ++
	The alignment of the label within the module, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*justify*: ++
	typeof: string ++
	The alignment of the text within the module's label, allowing options 'left', 'right', or 'center' to define the positioning.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right-click on the module.

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

*{percentage}*: Percentage of memory in use.

*{swapPercentage}*: Percentage of swap in use.

*{total}*: Amount of total memory available in GiB.

*{swapTotal}*: Amount of total swap available in GiB.

*{used}*: Amount of used memory in GiB.

*{swapUsed}*: Amount of used swap in GiB.

*{avail}*: Amount of available memory in GiB.

*{swapAvail}*: Amount of available swap in GiB.

# EXAMPLES

```
"memory": {
	"interval": 30,
	"format": "{}% ",
	"max-length": 10
}
```

## FORMATTED MEMORY VALUES

```
"memory": {
	"interval": 30,
	"format": "{used:0.1f}G/{total:0.1f}G "
}
```

# STYLE

- *#memory*
