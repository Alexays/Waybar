waybar-sway-layout(5)

# NAME

waybar - sway layout module

# DESCRIPTION

The *layout* module displays the current keyboard layout of Sway

# CONFIGURATION

Addressed by *sway/layout*

*format*: ++
	typeof: string ++
	default: "{}" ++
	The format, how information should be displayed.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

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
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	default: "{long}" ++
	The format of the information displayed in the tooltip.

# FORMAT REPLACEMENTS

*{long}*: The complete layout and variant indication (e.g "English (US)")

*{short}*: The layout short name (e.g "en")

*{variant}*: The variant short name (e.g "us")

# EXAMPLES

```
"sway/layout": {
	"tooltip-format": "{long}",
	"format": "{short} ({variant})",
	"keyboard_id": "1:1:AT_Translated_Set_2_keyboard"
}
```

# STYLE

- *#layout*
