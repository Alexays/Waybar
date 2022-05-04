waybar-bluetooth(5)

# NAME

waybar - bluetooth module

# DESCRIPTION

The *bluetooth* module displays information about the bluetooth adapter and its connections.

# CONFIGURATION

Addressed by *bluetooth*

*adapter-alias*: ++
	typeof: string ++
	Use the adapter with the defined alias (name). Otherwise select a random adapter to display. Recommended to define when there is more than 1 adapter available to the system (use ```bluetoothctl show``` to show all available adapters).

*format-device-preference*: ++
	typeof: array ++
	A ranking of bluetooth devices, addressed by their alias (name). The order is *first displayed* to *last displayed*. If this config option is not defined or none of the devices in the list are connected, it will fall back to showing the last connected device. A devices alias can be manually changed using the ```bluetoothctl set-alias``` command.

*format*: ++
	typeof: string  ++
	default: * {status}* ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-disabled*: ++
	typeof: string ++
	This format is used when the displayed adapter is disabled.

*format-off*: ++
	typeof: string ++
	This format is used when the displayed adapter is turned off.

*format-on*: ++
	typeof: string ++
	This format is used when the displayed adapter is turned on with no devices connected.

*format-connected*: ++
	typeof: string ++
	This format is used when the displayed adapter is connected to at least 1 device.

*format-connected-battery*: ++
	typeof: string ++
	This format is used when the selected connected device, defined by the config option *format-device-preference*, provides is battery percentage. This needs the experimental features of bluez to be enabled to work.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

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
	default: *true* ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	The format, how information should be displayed in the tooltip. This format is used when other formats aren't specified.

*tooltip-format-disabled*: ++
	typeof: string ++
	This format is used when the displayed adapter is disabled.

*tooltip-format-off*: ++
	typeof: string ++
	This format is used when the displayed adapter is turned off.

*tooltip-format-on*: ++
	typeof: string ++
	This format is used when the displayed adapter is turned on with no devices connected.

*tooltip-format-connected*: ++
	typeof: string ++
	This format is used when the displayed adapter is connected to at least 1 device.

*tooltip-format-connected-battery*: ++
	typeof: string ++
	This format is used when the selected connected device, defined by the config option *format-device-preference*, provides is battery percentage. This needs the experimental features of bluez to be enabled to work.

*tooltip-format-enumerate-connected*: ++
	typeof: string ++
	This format is used to define how each connected device should be displayed within the *device_enumerate* format replacement in the tooltip menu.

*tooltip-format-enumerate-connected-battery*: ++
	typeof: string ++
	This format is used to define how each connected device with a battery should be displayed within the *device_enumerate* format replacement in the tooltip menu. When this config option is not defined, the *tooltip-format-enumerate-connected* format will be used also for the connected devices with a battery.


# FORMAT REPLACEMENTS

*{status}*: Status of the bluetooth device.

*{num_connections}*: Number of connections the selected adapter has.

*{adapter_address}*: Address of the selected adapter.

*{adapter_address_type}*: Address type of the selected adapter.

*{adapter_alias}*: Alias of the selected adapter. By default equal to the *adapter_name* but can be changed by the user when there are conflicts.

*{device_address}*: Address of the current selected selected connected device.

*{device_address_type}*: Address type of the current selected selected connected device.

*{device_alias}*: Alias of the current selected connected device. By default equal to the *device_name* but can be changed by the user when there are conflicts.

*{device_battery_percentage}*: Battery percentage of the current selected device if available. Only use in the *format-connected-battery* and *tooltip-format-connected-battery*.

*{device_enumerate}*: Show a list of all connected devices in the tooltip, each on a seperate line. Only applicable in the *connected* state. Define the format of each device with the *tooltip-format-enumerate-connected* or/and *tooltip-format-enumerate-connected-battery* config options.

# EXAMPLES

```
"bluetooth": {
	// "adapter-alias": "adapter1", // specify the adapter alias (name) if there are more than 1 on the system
	"format": " {status}",
	"format-connected": " {num_connections} connected",
	"tooltip-format": "{adapter_alias}\t{adapter_address}",
	"tooltip-format-connected": "{adapter_alias}\t{adapter_address}\n\n{device_enumerate}",
	"tooltip-format-enumerate-connected": "{device_alias}\t{device_address}",
	"tooltip-format-enumerate-connected-battery": "{device_alias}\t{device_address}\t{device_battery_percentage}%"
}
```

```
"bluetooth": {
	"format": " {status}",
        "format-connected": " {device_alias}",
        "format-connected-battery": " {device_alias} {device_battery_percentage}%",
        // "format-device-preference": [ "alias1", "alias2" ], // preference list deciding which device to show in format-connected format-connected-battery
	"tooltip-format": "{adapter_alias}\t{adapter_address}\n\n{num_connections} connected\n\n{device_enumerate}",
	"tooltip-format-enumerate-connected": "{device_alias}\t{device_address}",
	"tooltip-format-enumerate-connected-battery": "{device_alias}\t{device_address}\t{device_battery_percentage}%"
}
```

# STYLE

- *#bluetooth*
- *#bluetooth.disabled*
- *#bluetooth.off*
- *#bluetooth.on*
- *#bluetooth.connected*
- *#bluetooth.discoverable*
- *#bluetooth.discovering*
- *#bluetooth.pairable*
