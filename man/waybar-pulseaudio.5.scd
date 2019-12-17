waybar-pulseaudio(5)

# NAME

waybar - pulseaudio module

# DESCRIPTION

The *pulseaudio* module displays the current volume reported by PulseAudio.

Additionally you can control the volume by scrolling *up* or *down* while the cursor is over the module.

# CONFIGURATION

*format*: ++
	typeof: string  ++
	default: {volume}% ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-bluetooth*: ++
	typeof: string ++
	This format is used when using bluetooth speakers.

*format-muted*: ++
	typeof: string ++
	This format is used when the sound is muted.

*format-source*: ++
	typeof: string ++
	default: {volume}% ++
	This format used for the source.

*format-source-muted*: ++
	typeof: string ++
	This format is used when the source is muted.

*format-icons*: ++
	typeof: array ++
	Based on the current port-name and volume, the corresponding icon gets selected. The order is *low* to *high*. See *Icons*.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*states*: ++
	typeof: array ++
	A number of volume states which get activated on certain volume levels. See *waybar-states(5)*

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*scroll-step*: ++
	typeof: float ++
	default: 1.0 ++
	The speed in which to change the volume when scrolling.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right clicked on the module.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when scrolling up on the module. This replaces the default behaviour of volume control.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module. This replaces the default behaviour of volume control.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{desc}*: Pulseaudio port's description, for bluetooth it'll be the device name.

*{volume}*: Volume in percentage.

*{icon}*: Icon, as defined in *format-icons*.

*{format_source}*: Source format, *format-source*, *format-source-muted*.

# ICONS:

The following strings for *format-icons* are supported.
If they are found in the current PulseAudio port name, the corresponding icons will be selected.

- *default* (Shown, when no other port is found)
- *headphones*
- *speaker*
- *hdmi*
- *headset*
- *handsfree*
- *portable*
- *car*
- *hifi*
- *phone*

# EXAMPLES

```
"pulseaudio": {
	"format": "{volume}% {icon}",
	"format-bluetooth": "{volume}% {icon}",
	"format-muted": "",
	"format-icons": {
		"headphones": "",
		"handsfree": "",
		"headset": "",
		"phone": "",
		"portable": "",
		"car": "",
		"default": ["", ""]
	},
	"scroll-step": 1,
	"on-click": "pavucontrol"
}
```

# STYLE

- *#pulseaudio*
- *#pulseaudio.bluetooth*
- *#pulseaudio.muted*
