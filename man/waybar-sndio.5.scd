waybar-sndio(5)

# NAME

waybar - sndio module

# DESCRIPTION

The *sndio* module displays the current volume reported by sndio(7).

Additionally, you can control the volume by scrolling *up* or *down* while the
cursor is over the module, and clicking on the module toggles mute.

# CONFIGURATION

*format*: ++
	typeof: string  ++
	default: {volume}% ++
	The format for how information should be displayed.

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

*scroll-step*: ++
	typeof: int ++
	default: 5 ++
	The speed in which to change the volume when scrolling.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.
	This replaces the default behaviour of toggling mute.

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
	Command to execute when scrolling up on the module. ++
	This replaces the default behaviour of volume control.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module. ++
	This replaces the default behaviour of volume control.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

# FORMAT REPLACEMENTS

*{volume}*: Volume in percentage.

*{raw_value}*: Volume as value reported by sndio.

# EXAMPLES

```
"sndio": {
	"format": "{raw_value} 🎜",
	"scroll-step": 3
}
```

# STYLE

- *#sndio*
- *#sndio.muted*
