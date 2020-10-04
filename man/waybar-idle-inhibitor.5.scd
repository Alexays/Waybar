waybar-idle-inhibitor(5)

# NAME

waybar - idle_inhibitor module

# DESCRIPTION

The *idle_inhibitor* module can inhibiting the idle behavior such as screen blanking, locking, and
screensaving, also known as "presentation mode".

# CONFIGURATION

*format*: ++
	typeof: string ++
	The format, how the state should be displayed.

*format-icons*: ++
	typeof: array ++
	Based on the current state, the corresponding icon gets selected.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{status}*: status (*activated* or *deactivated*)

*{icon}*: Icon, as defined in *format-icons*

# EXAMPLES

```
"idle_inhibitor": {
	"format": "{icon}",
	"format-icons": {
	    "activated": "",
	    "deactivated": ""
	}
}
```
