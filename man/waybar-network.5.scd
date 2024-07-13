waybar-network(5)

# NAME

waybar - network module

# DESCRIPTION

The *network* module displays information about the current network connections.

# CONFIGURATION

Addressed by *network*

*interface*: ++
	typeof: string ++
	Use the defined interface instead of auto-detection. Accepts wildcard.

*interval*: ++
	typeof: integer ++
	default: 60 ++
	The interval in which the network information gets polled (e.g. signal strength).

*family*: ++
	typeof: string ++
	default: *ipv4* ++
	The address family that is used for the format replacement {ipaddr} and to determine if a network connection is present.

*format*: ++
	typeof: string  ++
	default: *{ifname}* ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-ethernet*: ++
	typeof: string ++
	This format is used when an ethernet interface is displayed.

*format-wifi*: ++
	typeof: string ++
	This format is used when a wireless interface is displayed.

*format-linked*: ++
	typeof: string ++
	This format is used when a linked interface with no IP address is displayed.

*format-disconnected*: ++
	typeof: string ++
	This format is used when the displayed interface is disconnected.

*format-disabled*: ++
	typeof: string ++
	This format is used when the displayed interface is disabled.

*format-icons*: ++
	typeof: array/object ++
	Based on the current signal strength, the corresponding icon gets selected. ++
	The order is *low* to *high*. Or by the state if it is an object.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

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
	default: *true* ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	The format, how information should be displayed in the tooltip. This format is used when other formats aren't specified.

*tooltip-format-ethernet*: ++
	typeof: string ++
	This format is used when an ethernet interface is displayed.

*tooltip-format-wifi*: ++
	typeof: string ++
	This format is used when a wireless interface is displayed.

*tooltip-format-disconnected*: ++
	typeof: string ++
	This format is used when the displayed interface is disconnected.

*tooltip-format-disabled*: ++
	typeof: string ++
	This format is used when the displayed interface is disabled.

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

*{ifname}*: Name of the network interface.

*{ipaddr}*: The first IP of the interface.

*{gwaddr}*: The default gateway for the interface

*{netmask}*: The subnetmask corresponding to the IP.

*{cidr}*: The subnetmask corresponding to the IP in CIDR notation.

*{essid}*: Name (SSID) of the wireless network.

*{bssid}*: MAC address (BSSID) of the wireless access point.

*{signalStrength}*: Signal strength of the wireless network.

*{signaldBm}*: Signal strength of the wireless network in dBm.

*{frequency}*: Frequency of the wireless network in MHz.

*{bandwidthUpBits}*: Instant up speed in bits/seconds.

*{bandwidthDownBits}*: Instant down speed in bits/seconds.

*{bandwidthTotalBits}*: Instant total speed in bits/seconds.

*{bandwidthUpOctets}*: Instant up speed in octets/seconds.

*{bandwidthDownOctets}*: Instant down speed in octets/seconds.

*{bandwidthTotalOctets}*: Instant total speed in octets/seconds.

*{bandwidthUpBytes}*: Instant up speed in bytes/seconds.

*{bandwidthDownBytes}*: Instant down speed in bytes/seconds.

*{bandwidthTotalBytes}*: Instant total speed in bytes/seconds.

*{icon}*: Icon, as defined in *format-icons*.

# EXAMPLES

```
"network": {
	"interface": "wlp2s0",
	"format": "{ifname}",
	"format-wifi": "{essid} ({signalStrength}%) ",
	"format-ethernet": "{ifname} ",
	"format-disconnected": "", //An empty format will hide the module.
	"format-disconnected": "",
	"tooltip-format": "{ifname}",
	"tooltip-format-wifi": "{essid} ({signalStrength}%) ",
	"tooltip-format-ethernet": "{ifname} ",
	"tooltip-format-disconnected": "Disconnected",
	"max-length": 50
}
```

# STYLE

- *#network*
- *#network.disconnected*
- *#network.disabled*
- *#network.linked*
- *#network.ethernet*
- *#network.wifi*
