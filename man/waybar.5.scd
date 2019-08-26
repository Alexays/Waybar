waybar(5)

# NAME

waybar - configuration file

# DESCRIPTION

The configuration uses the JSON file format and is named *config*.

Valid directories for this file are:

- *~/.config/waybar/*
- *~/waybar/*
- */etc/xdg/waybar/*

A good starting point is the default config found at https://github.com/Alexays/Waybar/blob/master/resources/config.
Also a minimal example config can be found on the at the bottom of this man page.

# BAR CONFIG

*layer* ++
	typeof: string ++
	default: bottom ++
	Decide if the bar is displayed in front of the windows or behind them.

*output* ++
	typeof: string|array ++
	Specifies on which screen this bar will be displayed.

*position* ++
	typeof: string ++
	default: top ++
	Bar position, can be `top`,`bottom`,`left`,`right`.

*height* ++
	typeof: integer ++
	Height to be used by the bar if possible, leave blank for a dynamic value.

*width* ++
	typeof: integer ++
	Width to be used by the bar if possible, leave blank for a dynamic value.

*modules-left* ++
	typeof: array ++
	Modules that will be displayed on the left.

*modules-center* ++
	typeof: array ++
	Modules that will be displayed in the center.

*modules-right* ++
	typeof: array
	Modules that will be displayed on the right.

*margin* ++
	typeof: string ++
	Margins value using the css format without units.

*margin-<top\|left\|bottom\|right>* ++
	typeof: integer ++
	Margins value without units.

*name* ++
	typeof: string ++
	Optional name added as a CSS class, for styling multiple waybars.

# Module format

You can use PangoMarkupFormat (See https://developer.gnome.org/pango/stable/PangoMarkupFormat.html#PangoMarkupFormat).

e.g.

```
"format": "<span style=\"italic\">{}</span>"
```
# MULTIPLE INSTANCES OF A MODULE

If you want to have a second instance of a module, you can suffix it by a '#' and a custom name.
For example if you want a second battery module, you can add `"battery#bat2"` to your modules.
To configure the newly added module, you then also add a module configuration with the same name.

This could then look something like this *(this is an incomplete example)*:

```
"modules-right": ["battery", "battery#bat2"],
"battery": {
	"bat": "BAT1"
},
"battery#bat2": {
	"bat": "BAT2"
}
```

# MINIMAL CONFIG

A minimal *config* file could look like this:

```
{
	"layer": "top",
	"modules-left": ["sway/workspaces", "sway/mode"],
	"modules-center": ["sway/window"],
	"modules-right": ["battery", "clock"],
	"sway/window": {
		"max-length": 50
	},
	"battery": {
		"format": "{capacity}% {icon}",
		"format-icons": ["", "", "", "", ""]
	},
	"clock": {
		"format-alt": "{:%a, %d. %b  %H:%M}"
	}
}
```

# MULTI OUTPUT CONFIG

## Limit a configuration to some outputs

```
{
	"layer": "top",
	"output": "eDP-1",
	"modules-left": ["sway/workspaces", "sway/mode"],
	...

}

```

```
{
	"layer": "top",
	"output": ["eDP-1", "VGA"],
	"modules-left": ["sway/workspaces", "sway/mode"],
	...
}

```

## Configuration of multiple outputs

Don't specify an output to create multiple bars on the same screen.

```
[{
	"layer": "top",
	"output": "eDP-1",
	"modules-left": ["sway/workspaces", "sway/mode"],
	...
}, {
	"layer": "top",
	"output": "VGA",
	"modules-right": ["clock"],
	...
}]

```

## Rotating modules

When positioning Waybar on the left or right side of the screen, sometimes it's useful to be able to rotate the contents of a module so the text runs vertically. This can be done using the "rotate" property of the module. Example:

```
{
	"clock": {
		"rotate": 90
	}
}
```

Valid options for the "rotate" property are: 0, 90, 180 and 270.
