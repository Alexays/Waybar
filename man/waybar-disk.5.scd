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
	A number of disk utilization states that get activated on certain percentage thresholds (percentage_used). See *waybar-states(5)*.

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

*tooltip-format*: ++
	typeof: string ++
	default: "{used} out of {total} used ({percentage_used}%)" ++
	The format of the information displayed in the tooltip.

*unit*: ++
	typeof: string ++
	Use with specific_free, specific_used, and specific_total to force calculation to always be in a certain unit. Accepts kB, kiB, MB, Mib, GB, GiB, TB, TiB.

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

*{percentage_used}*: Percentage of disk in use.

*{percentage_free}*: Percentage of free disk space

*{total}*: Total amount of space on the disk, partition, or mountpoint. Automatically selects unit based on size remaining.

*{used}*: Amount of used disk space. Automatically selects unit based on size remaining.

*{free}*: Amount of available disk space for normal users. Automatically selects unit based on size remaining.

*{path}*: The path specified in the configuration.

*{specific_total}*: Total amount of space on the disk, partition, or mountpoint in a specific unit. Defaults to bytes.

*{specific_used}*: Amount of used disk space in a specific unit. Defaults to bytes.

*{specific_free}*: Amount of available disk space for normal users in a specific unit. Defaults to bytes.

# EXAMPLES

```
"disk": {
	"interval": 30,
	"format": "{percentage_free}% free on {path}",
}
```

```
"disk": {
	"interval": 30,
	"format": "{specific_free:0.2f} GB out of {specific_total:0.2f} GB available. Alternatively {free} out of {total} available",
	"unit": "GB"
	// 1434.25 GB out of 2000.00 GB available. Alternatively 1.4TiB out of 1.9TiB available.
}
```

# STYLE

- *#disk*
