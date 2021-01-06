waybar-memory(5)

# NAME

waybar - memory module

# DESCRIPTION

The *memory* module displays the current memory utilization.

# CONFIGURATION

Addressed by *memory*

*interval*: ++
	typeof: integer++
	default: 30 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string ++
	default: {percentage}% ++
	The format, how information should be displayed.

*states*: ++
	typeof: array ++
	A number of memory utilization states which get activated on certain percentage thresholds. See *waybar-states(5)*.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{percentage}*: Percentage of memory in use.

*{total}*: Amount of total memory available in GiB.

*{used}*: Amount of used memory in GiB.

*{avail}*: Amount of available memory in GiB.

# EXAMPLES

```
"memory": {
	"interval": 30,
	"format": "{}% ",
	"max-length": 10
}
```

## FORMATTED MEMORY VALUES

```
"memory": {
	"interval": 30,
	"format": "{used:0.1f}G/{total:0.1f}G "
}
```

# STYLE

- *#memory*
