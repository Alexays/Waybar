waybar-cpu(5)

# NAME

waybar - cpu module

# DESCRIPTION

The *cpu* module displays the current cpu utilization.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	default: 10 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string  ++
	default: {usage}% ++
	The format, how information should be displayed. On {} data gets inserted.

*states*: ++
	typeof: array ++
	A number of cpu usage states which get activated on certain usage levels. See *waybar-states(5)*.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{load}*: Current cpu load.

*{usage}*: Current cpu usage.

# EXAMPLE

```
"cpu": {
	"interval": 10,
	"format": "{}% ",
	"max-length": 10
}
```

# STYLE

- *#cpu*
