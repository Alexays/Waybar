waybar-battery(5)

# NAME

waybar - battery module

# DESCRIPTION

The *battery* module displays the current capacity and state (eg. charging) of your battery.

# CONFIGURATION

*bat*: ++
	typeof: string ++
	The battery to monitor, as in /sys/class/power_supply/ instead of auto detect.

*adapter*: ++
	typeof: string ++
	The adapter to monitor, as in /sys/class/power_supply/ instead of auto detect.

*full-at*: ++
	typeof: integer ++
	Define the max percentage of the battery, for when you've set the battery to stop charging at a lower level to save it. For example, if you've set the battery to stop at 80% that will become the new 100%.

*design-capacity*: ++
	typeof: bool ++
	default: false ++
	Option to use the battery design capacity instead of its current maximal capacity.

*interval*: ++
	typeof: integer ++
	default: 60 ++
	The interval in which the information gets polled.

*states*: ++
	typeof: object ++
	A number of battery states which get activated on certain capacity levels. See *waybar-states(5)*.

*format*: ++
	typeof: string ++
	default: {capacity}% ++
	The format, how information should be displayed.

*format-time*: ++
	typeof: string ++
	default: {H} h {M} min ++
	The format, how the time should be displayed.

*format-icons*: ++
	typeof: array/object ++
	Based on the current capacity, the corresponding icon gets selected. ++
	The order is *low* to *high*. Or by the state if it is an object.

*max-length*: ++
	typeof: integer++
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
	typeof: integer++
	Positive value to rotate the text label.

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

*bat-compatibility*: ++
	typeof: bool ++
	default: false ++
	Option to enable battery compatibility if not detected.

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

*{capacity}*: Capacity in percentage

*{power}*: Power in watts

*{icon}*: Icon, as defined in *format-icons*.

*{time}*: Estimate of time until full or empty. Note that this is based on the power draw at the last refresh time, not an average.

*{cycles}*: Amount of charge cycles the highest-capacity battery has seen. *(Linux only)*

*{health}*: The percentage of the highest-capacity battery's original maximum charge it can still hold.

# TIME FORMAT

The *battery* module allows you to define how time should be formatted via *format-time*.

The three arguments are:
*{H}*: Hours
*{M}*: Minutes
*{m}*: Zero-padded minutes

# CUSTOM FORMATS

The *battery* module allows one to define custom formats based on up to two factors. The best-fitting format will be selected.

*format-<state>*: With *states*, a custom format can be set depending on the capacity of your battery.

*format-<status>*: With the status, a custom format can be set depending on the status in /sys/class/power_supply/<bat>/status (in lowercase).

*format-<status>-<state>*: You can also set a custom format depending on both values.

# STATES

- Every entry (*state*) consists of a *<name>* (typeof: *string*) and a *<value>* (typeof: *integer*).
- The state can be addressed as a CSS class in the *style.css*. The name of the CSS class is the *<name>* of the state.	Each class gets activated when the current capacity is equal to or below the configured *<value>*.
- Also each state can have its own *format*. Those can be configured via *format-<name>*. Or if you want to differentiate a bit more even as *format-<status>-<state>*. For more information see *custom-formats*.



# EXAMPLES

```
"battery": {
	"bat": "BAT2",
	"interval": 60,
	"states": {
		"warning": 30,
		"critical": 15
	},
	"format": "{capacity}% {icon}",
	"format-icons": ["", "", "", "", ""],
	"max-length": 25
}
```

# STYLE

- *#battery*
- *#battery.<status>*
	- *<status>* is the value of /sys/class/power_supply/<bat>/status in lowercase.
- *#battery.<state>*
	- *<state>* can be defined in the *config*. For more information see *states*.
- *#battery.<status>.<state>*
	- Combination of both *<status>* and *<state>*.

The following classes are applied to the entire Waybar rather than just the
battery widget:

- *window#waybar.battery-<state>*
	- *<state>* can be defined in the *config*, as previously mentioned.

