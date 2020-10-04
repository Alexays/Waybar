waybar-bluetooth(5)

# NAME

waybar - bluetooth module

# DESCRIPTION

The *bluetooth* module displays information about the status of the device's bluetooth device.

# CONFIGURATION

Addressed by *bluetooth*

*interval*: ++
	typeof: integer ++
	default: 60 ++
	The interval in which the bluetooth state gets updated.

*format*: ++
	typeof: string  ++
	default: *{icon}* ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-icons*: ++
	typeof: array/object ++
	Based on the device status, the corresponding icon gets selected. ++
	The order is *low* to *high*. Or by the state if it is an object.

*tooltip*: ++
	typeof: bool ++
	default: *true* ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{status}*: Status of the bluetooth device.

*{icon}*: Icon, as defined in *format-icons*.

# EXAMPLES

```
"bluetooth": {
	"format": "{icon}",
	"format-alt": "bluetooth: {status}",
	"interval": 30,
	"format-icons": {
		"enabled": "",
		"disabled": ""
	},
	"tooltip-format": "{status}"
}
```

# STYLE

- *#bluetooth*
