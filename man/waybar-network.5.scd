waybar-network(5)

# NAME

waybar - network module

# DESCRIPTION

The *network* module displays information about the current network connections.

# CONFIGURATION

Addressed by *network*

*interface*: ++
	typeof: string ++
	Use the defined interface instead of auto detection. Accepts wildcard.

*interval*: ++
	typeof: integer ++
	default: 60 ++
	The interval in which the network information gets polled (e.g. signal strength).

*format*: ++
	typeof: string  ++
	default: *{ifname}* ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-ethernet*: ++
	typeof: string ++
	This format is used when a ethernet interface is displayed.

*format-wifi*: ++
	typeof: string ++
	This format is used when a wireless interface is displayed.

*format-linked*: ++
	typeof: string ++
	This format is used when a linked interface with no ip address is displayed.

*format-disconnected*: ++
	typeof: string ++
	This format is used when the displayed interface is disconnected.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

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

*tooltip-format-ethernet*: ++
	typeof: string ++
	This format is used when a ethernet interface is displayed.

*tooltip-format-wifi*: ++
	typeof: string ++
	This format is used when a wireless interface is displayed.

*tooltip-format-disconnected*: ++
	typeof: string ++
	This format is used when the displayed interface is disconnected.

# FORMAT REPLACEMENTS

*{ifname}*: Name of the network interface.

*{ipaddr}*: The first IP of the interface.

*{netmask}*: The subnetmask corresponding to the IP.

*{cidr}*: The subnetmask corresponding to the IP in CIDR notation.

*{essid}*: Name (SSID) of the wireless network.

*{signalStrength}*: Signal strength of the wireless network.

*{signaldBm}*: Signal strength of the wireless network in dBm.

*{frequency}*: Frequency of the wireless network in MHz.

*{bandwidthUpBits}*: Instant up speed in bits/seconds.

*{bandwidthDownBits}*: Instant down speed in bits/seconds.

*{bandwidthUpOctets}*: Instant up speed in octets/seconds.

*{bandwidthDownOctets}*: Instant down speed in octets/seconds.

# EXAMPLES

```
"network": {
	"interface": "wlp2s0",
	"format": "{ifname}",
	"format-wifi": "{essid} ({signalStrength}%) ",
	"format-ethernet": "{ifname} ",
	"format-disconnected": "", //An empty format will hide the module.
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
- *#network.linked*
- *#network.ethernet*
- *#network.wifi*
