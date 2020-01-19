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

*critical-threshold*: ++
	typeof: integer ++
	The threshold before it is considered critical (Celcius).

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
	The format (Celcius/Farenheit) in which the temperature should be displayed.

*format-icons*: ++
	typeof: array ++
	Based on the current temperature (Celcius) and *critical-threshold* if available, the corresponding icon gets selected. The order is *low* to *high*.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in characters the module should display.

*on-click*: ++
	typeof: string ++
	Command to execute when you clicked on the module.

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

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{temperatureC}*: Temperature in Celcius.

*{temperatureF}*: Temperature in Fahrenheit.

# EXAMPLES

```
 "temperature": {
	// "thermal-zone": 2,
	// "hwmon-path": "/sys/class/hwmon/hwmon2/temp1_input",
	// "critical-threshold": 80,
	// "format-critical": "{temperatureC}°C ",
	"format": "{temperatureC}°C "
}
```

# STYLE

- *#temperature*
- *#temperature.critical*
