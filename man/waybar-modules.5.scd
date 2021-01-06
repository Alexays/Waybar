waybar-modules(5)

# OVERVIEW

This page describes some general module configuration options.
Most modules support these.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	The interval in which information gets polled.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*on-click*: ++
	typeof: string ++
	Command to execute when the module is clicked.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when the module is right clicked.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when performing a scroll up on the module.

*on-scroll-down*: ++
	typeof: string
	Command to execute when performing a scroll down on the module.

*smooth-scrolling-threshold*: ++
	typeof: double
	Threshold to be used when scrolling.

*tooltip*: ++
	typeof: bool ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	The format, how information should be displayed in the tooltip.
	This format is used when other formats aren't specified.
