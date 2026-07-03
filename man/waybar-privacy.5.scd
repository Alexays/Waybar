waybar-privacy(5)

# NAME

waybar - privacy module

# DESCRIPTION

The *privacy* module displays if any application is capturing audio, sharing ++
the screen or playing audio.

# CONFIGURATION

*icon-spacing*: ++
	typeof: integer ++
	default: 4 ++
	The spacing between each privacy icon.

*icon-size*: ++
	typeof: integer ++
	default: 20 ++
	The size of each privacy icon.

*transition-duration*: ++
	typeof: integer ++
	default: 250 ++
	Option to disable tooltip on hover.

*modules* ++
	typeof: array of objects ++
	default: [{"type": "screenshare"}, {"type": "audio-in"}] ++
	Which privacy modules to monitor. See *MODULES CONFIGURATION* for++
	more information.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

*ignore-monitor* ++
	typeof: bool ++
	default: true ++
	Ignore streams with *stream.monitor* property.

*ignore* ++
	typeof: array of objects ++
	default: [] ++
	Additional streams to be ignored. See *IGNORE CONFIGURATION* for++
	more information.

# MODULES CONFIGURATION

*type*: ++
	typeof: string ++
	values: "screenshare", "audio-in", "audio-out" ++
	Specifies which module to use and configure.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-icon-size*: ++
	typeof: integer ++
	default: 24 ++
	The size of each icon in the tooltip.

# IGNORE CONFIGURATION

*type*: ++
	typeof: string

*name*: ++
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

- *#privacy*
- *#privacy-item*
- *#privacy-item.screenshare*
- *#privacy-item.audio-in*
- *#privacy-item.audio-out*
