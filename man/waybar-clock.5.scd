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
	The format, how the date and time should be displayed. ++
	It uses the format of the date library. See https://howardhinnant.github.io/date/date.html#to_stream_formatting for details.

*timezone*: ++
	typeof: string ++
	default: inferred local timezone ++
	The timezone to display the time in, e.g. America/New_York. ++
	This field will be ignored if *timezones* field is set and have at least one value.

*timezones*: ++
	typeof: list of strings ++
	A list of timezones to use for time display, changed using the scroll wheel. ++
	Use "" to represent the system's local timezone.  Using %Z in the format or tooltip format is useful to track which time zone is currently displayed.

*locale*: ++
	typeof: string ++
	default: inferred from current locale ++
	A locale to be used to display the time. Intended to render times in custom timezones with the proper language and format.

*today-format*: ++
	typeof: string ++
	default: <b><u>{}</u></b> ++
	The format of today's date in the calendar.

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

View all valid format options in *strftime(3)*.

# FORMAT REPLACEMENTS

*{calendar}*: Current month calendar
*{timezoned_time_list}*: List of time in the rest timezones, if more than one timezone is set in the config

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
