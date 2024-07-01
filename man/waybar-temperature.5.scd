waybar-temperature(5)

# NAME

waybar - temperature module

# DESCRIPTION

The *temperature* module displays the current temperature from a thermal zone.

# CONFIGURATION

Addressed by *temperature*

*thermal-zone*: ++
	typeof: integer ++
	The thermal zone, as in */sys/class/thermal/*.

*hwmon-path*: ++
	typeof: string ++
	The temperature path to use, e.g. */sys/class/hwmon/hwmon2/temp1_input* instead of one in */sys/class/thermal/*.
	This can also be an array of strings. In this case, waybar will check each item in the array and use the first valid one.
	This is suitable if you want to share the same configuration file among different machines with different hardware configurations.

*hwmon-path-abs*: ++
	typeof: string ++
	The path of the hwmon-directory of the device, e.g. */sys/devices/pci0000:00/0000:00:18.3/hwmon*. (Note that the subdirectory *hwmon/hwmon#*, where *#* is a number is not part of the path!) Has to be used together with *input-filename*.
	This can also be an array of strings, for which, it just works like *hwmon-path*.

*input-filename*: ++
	typeof: string ++
	The temperature filename of your *hwmon-path-abs*, e.g. *temp1_input*

*critical-threshold*: ++
	typeof: integer ++
	The threshold before it is considered critical (Celsius).

*interval*: ++
	typeof: integer ++
	default: 10 ++
	The interval in which the information gets polled.

*format-critical*: ++
	typeof: string ++
	The format to use when temperature is considered critical

*format*: ++
	typeof: string  ++
	default: {temperatureC}°C ++
	The format (Celsius/Fahrenheit/Kelvin) in which the temperature should be displayed.

*format-icons*: ++
	typeof: array ++
	Based on the current temperature (Celsius) and *critical-threshold* if available, the corresponding icon gets selected. The order is *low* to *high*.

*tooltip-format*: ++
	typeof: string  ++
	default: {temperatureC}°C ++
	The format for the tooltip

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in characters the module should display.

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
	Command to execute when you click on the module.

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

*{temperatureC}*: Temperature in Celsius.

*{temperatureF}*: Temperature in Fahrenheit.

*{temperatureK}*: Temperature in Kelvin.

# EXAMPLES

```
 "temperature": {
	// "thermal-zone": 2,
	// "hwmon-path": ["/sys/class/hwmon/hwmon2/temp1_input", "/sys/class/thermal/thermal_zone0/temp"],
	// "critical-threshold": 80,
	// "format-critical": "{temperatureC}°C ",
	"format": "{temperatureC}°C "
}
```

# STYLE

- *#temperature*
- *#temperature.critical*
