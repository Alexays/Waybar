waybar-clock(5)

# NAME

waybar - clock module

# DESCRIPTION

The *clock* module displays the current date and time.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	default: 60 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string ++
	default: {:%H:%M} ++
	The format, how the date and time should be displayed.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right clicked on the module.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when scrolling up on the module.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

View all valid format options in *strftime(3)*.

# EXAMPLES

```
"clock": {
    "interval": 60,
    "format": "{:%H:%M}",
    "max-length": 25
}
```

# STYLE

- *#clock*
