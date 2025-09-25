waybar-privacy(5)

# NAME

waybar - privacy module

# DESCRIPTION

The _privacy_ module displays if any application is capturing audio, sharing ++
the screen or playing audio.

# CONFIGURATION

_icon-spacing_: ++
typeof: integer ++
default: 4 ++
The spacing between each privacy icon.

_icon-size_: ++
typeof: integer ++
default: 20 ++
The size of each privacy icon.

_transition-duration_: ++
typeof: integer ++
default: 250 ++
Option to disable tooltip on hover.

_modules_ ++
typeof: array of objects ++
default: [{"type": "screenshare"}, {"type": "audio-in"}] ++
Which privacy modules to monitor. See _MODULES CONFIGURATION_ for++
more information.

_expand_: ++
typeof: bool ++
default: false ++
Enables this module to consume all left over space dynamically.

_ignore-monitor_ ++
typeof: bool ++
default: true ++
Ignore streams with _stream.monitor_ property.

_ignore_ ++
typeof: array of objects ++
default: [] ++
Additional streams to be ignored. See _IGNORE CONFIGURATION_ for++
more information.

# MODULES CONFIGURATION

_type_: ++
typeof: string ++
values: "screenshare", "audio-in", "audio-out", "camera" ++
Specifies which module to use and configure.

_icon-name_: ++
typeof: string ++
Specifies a custom icon for the module.

_format_: ++
typeof: string ++
Specifies a custom format for the module.

_tooltip_: ++
typeof: bool ++
default: true ++
Option to disable tooltip on hover.

_tooltip-icon-size_: ++
typeof: integer ++
default: 24 ++
The size of each icon in the tooltip.

# IGNORE CONFIGURATION

_type_: ++
typeof: string

_name_: ++
typeof: string

# EXAMPLES

```
"privacy": {
	"icon-spacing": 4,
	"icon-size": 18,
	"transition-duration": 250,
	"modules": [
		{
			"type": "screenshare",
			"tooltip": true,
			"tooltip-icon-size": 24
		},
		{
			"type": "audio-out",
			"tooltip": true,
			"tooltip-icon-size": 24
		},
		{
			"type": "audio-in",
			"tooltip": true,
			"tooltip-icon-size": 24
		}
	],
	"ignore-monitor": true,
	"ignore": [
		{
			"type": "audio-in",
			"name": "cava"
		},
		{
			"type": "screenshare",
			"name": "obs"
		}
	]
},
```

# STYLE

- _#privacy_
- _#privacy-item_
- _#privacy-item.screenshare_
- _#privacy-item.audio-in_
- _#privacy-item.audio-out_
- _#privacy-item.camera_
