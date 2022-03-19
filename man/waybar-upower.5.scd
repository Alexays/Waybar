waybar-upower(5)

# NAME

waybar - upower module

# DESCRIPTION

The *upower* module displays the main battery capacity with all other upower
compatible devices in the tooltip.

# CONFIGURATION

*icon-size*: ++
	typeof: integer ++
	default: 20 ++
	Defines the size of the icons.

*hide-if-empty*: ++
	typeof: bool ++
	default: true ++
	Defines visibility of the module if no devices can be found.

*tooltip*: ++
	typeof: bool ++
	defualt: true ++
	Option to disable tooltip on hover.

*tooltip-spacing*: ++
	typeof: integer ++
	default: 4 ++
	Defines the spacing between the tooltip device name and device battery ++
	status.

# EXAMPLES

```
"upower": {
	"icon-size": 20,
	"hide-if-empty": true,
	"tooltip": true,
	"tooltip-spacing": 20
}

```
