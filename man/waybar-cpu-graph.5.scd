waybar-cpu(5)

# NAME

waybar - cpu graph module

# DESCRIPTION

The *cpu graph* module displays a line graph with the CPU utilization.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	default: 10 ++
	The interval in which the information gets polled.

*width*: ++
	typeof: integer ++
	The length in pixels the module should display.

*y_offset*: ++
	typeof: integer ++
	The margin in pixels at the bottom of the module.

*datapoints*: ++
	typeof: integer ++
	How many data points to show.

*on-click*: ++
	typeof: string  ++
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

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# EXAMPLES

Basic configuration:

```
"cpu_graph": {
	"interval": 2,
	"width": 10,
	"y_offset": 4
}
```

# STYLE

- *#cpu_graph*
- *.cpu-intensive*
- *.cpu-high*
- *.cpu-moderate*
