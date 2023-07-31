waybar-disk(5)

# NAME

waybar - disk module

# DESCRIPTION

The *disk* module displays the current disk space used.

# CONFIGURATION

Addressed by *disk*

*path*: ++
	typeof: string ++
	default: "/" ++
	Any path residing in the filesystem or mountpoint for which the information should be displayed.

*interval*: ++
	typeof: integer++
	default: 30 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string ++
	default: "{percentage_used}%" ++
	The format, how information should be displayed.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*states*: ++
	typeof: object ++
	A number of disk utilization states which get activated on certain percentage thresholds (percentage_used). See *waybar-states(5)*.

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

*tooltip-format*: ++
	typeof: string ++
	default: "{used} out of {total} used ({percentage_used}%)" ++
	The format of the information displayed in the tooltip.

# FORMAT REPLACEMENTS

*{percentage_used}*: Percentage of disk in use.

*{percentage_free}*: Percentage of free disk space

*{total}*: Total amount of space on the disk, partition or mountpoint.

*{used}*: Amount of used disk space.

*{free}*: Amount of available disk space for normal users.

*{path}*: The path specified in the configuration.

# EXAMPLES

```
"disk": {
	"interval": 30,
	"format": "{percentage_free}% free on {path}",
}
```

# STYLE

- *#disk*
