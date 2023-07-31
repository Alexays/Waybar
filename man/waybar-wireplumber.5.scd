waybar-wireplumber(5)

# NAME

waybar - WirePlumber module

# DESCRIPTION

The *wireplumber* module displays the current volume reported by WirePlumber.

# CONFIGURATION

*format*: ++
	typeof: string ++
	default: *{volume}%* ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-muted*: ++
	typeof: string ++
	This format is used when the sound is muted.

*tooltip*: ++
	typeof: bool ++
	default: *true* ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	default: *{node_name}* ++
	The format of information displayed in the tooltip.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*states*: ++
	typeof: object ++
	A number of volume states which get activated on certain volume levels. See *waybar-states(5)*.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*min-length*: ++
	typeof: integer ++
	The minimum length in characters the module should take up.

*align*: ++
	typeof: float ++
	The alignment of the text, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*scroll-step*: ++
	typeof: float ++
	default: 1.0 ++
	The speed in which to change the volume when scrolling.

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
	Command to execute when scrolling up on the module. This replaces the default behaviour of volume control.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module. This replaces the default behaviour of volume control.

*max-volume*: ++
	typeof: float ++
	default: 100 ++
	The maximum volume that can be set, in percentage.

# FORMAT REPLACEMENTS

*{volume}*: Volume in percentage.

*{node_name}*: The node's nickname as reported by WirePlumber (*node.nick* property)

# EXAMPLES

```
"wireplumber": {
	"format": "{volume}%",
	"format-muted": "",
	"on-click": "helvum"
}
```

# STYLE

- *#wireplumber*
- *#wireplumber.muted*
