waybar-cpu(5)

# NAME

waybar - cpu module

# DESCRIPTION

The *cpu* module displays the current CPU utilization.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	default: 10 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string  ++
	default: {usage}% ++
	The format, how information should be displayed. On {} data gets inserted.

*format-icons*: ++
	typeof: array/object ++
	Based on the current usage, the corresponding icon gets selected. ++
	The order is *low* to *high*. Or by the state if it is an object.

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

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label (in 90 degree increments).

*states*: ++
	typeof: object ++
	A number of CPU usage states which get activated on certain usage levels. See *waybar-states(5)*.

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

# FORMAT REPLACEMENTS

*{load}*: Current CPU load.

*{usage}*: Current overall CPU usage.

*{usage*{n}*}*: Current CPU core n usage. Cores are numbered from zero, so first core will be {usage0} and 4th will be {usage3}.

*{avg_frequency}*: Current CPU average frequency (based on all cores) in GHz.

*{max_frequency}*: Current CPU max frequency (based on the core with the highest frequency) in GHz.

*{min_frequency}*: Current CPU min frequency (based on the core with the lowest frequency) in GHz.

*{icon}*: Icon for overall CPU usage.

*{icon*{n}*}*: Icon for CPU core n usage. Use like {icon0}.

# EXAMPLES

Basic configuration:

```
"cpu": {
	"interval": 10,
	"format": "{}% ",
	"max-length": 10
}
```

CPU usage per core rendered as icons:

```
"cpu": {
	"interval": 1,
	"format": "{icon0}{icon1}{icon2}{icon3} {usage:>2}% ",
	"format-icons": ["▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"],
},
```

# STYLE

- *#cpu*
