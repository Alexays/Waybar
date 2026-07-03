waybar-custom-graph(5)
# NAME

waybar - custom graph module

# DESCRIPTION

The *custom-graph* module displays a graph with the percentage output of a script.

# CONFIGURATION

Addressed by *custom-graph/<name>*

*exec*: ++
	typeof: string ++
	The path to the script, which should be executed.

*exec-if*: ++
	typeof: string ++
	The path to a script, which determines if the script in *exec* should be executed. ++
	*exec* will be executed if the exit code of *exec-if* equals 0.

*exec-on-event*: ++
	typeof: bool ++
	default: true ++
	If an event command is set (e.g. *on-click* or *on-scroll-up*) then re-execute the script after executing the event command.

*return-type*: ++
	typeof: string ++
	See *return-type*

*interval*: ++
	typeof: integer or float ++
	The interval (in seconds) in which the information gets polled. ++
	Minimum value is 0.001 (1ms). Values smaller than 1ms will be set to 1ms. ++
	Use *once* if you want to execute the module only on startup. ++
	You can update it manually with a signal. If no *interval* or *signal* is defined, it is assumed that the out script loops itself. ++
	If a *signal* is defined then the script will run once on startup and will only update with a signal.

*restart-interval*: ++
	typeof: integer or float ++
	The restart interval (in seconds). ++
	Minimum value is 0.001 (1ms). Values smaller than 1ms will be set to 1ms. ++
	Can't be used with the *interval* option, so only with continuous scripts. ++
	Once the script exits, it'll be re-executed after the *restart-interval*.

*signal*: ++
	typeof: integer ++
	The signal number used to update the module. ++
	The number is valid between 1 and N, where *SIGRTMIN+N* = *SIGRTMAX*. ++
	If no interval is defined then a signal will be the only way to update the module.

*format*: ++
	typeof: string ++
	default: {text} ++
	The format, how information should be displayed. On {text} data gets inserted.

*format-icons*: ++
	typeof: array ++
	Based on the set percentage, the corresponding icon gets selected. The order is *low* to *high*.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label (in 90 degree increments).

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right-click on the module.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

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
	The tooltip format. If specified, overrides any tooltip output from the script in *exec*. ++
	Uses the same format replacements as *format*.

*escape*: ++
	typeof: bool ++
	default: false ++
	Option to enable escaping of script output.

*menu*: ++
	typeof: string ++
	Action that popups the menu.

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file. There need to be an element of type
	GtkMenu with id *menu*

*menu-actions*: ++
	typeof: array ++
	The actions corresponding to the buttons of the menu.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# RETURN-TYPE

When *return-type* is set to *json*, Waybar expects the *exec*-script  to output its data in JSON format.
This should look like this:

```
{"text": "$text", "tooltip": "$tooltip", "class": "$class", "percentage": $percentage }
```

The *class* parameter also accepts an array of strings.

If nothing or an invalid option is specified, Waybar expects i3blocks style output. Values are *newline* separated.
This should look like this:

```
$text\\n$tooltip\\n$class*
```

*class* is a CSS class, to apply different styles in *style.css*

# FORMAT REPLACEMENTS

*{text}*: Output of the script.

*{percentage}* Percentage which can be set via a json return type.

*{icon}*: An icon from 'format-icons' according to percentage.

# EXAMPLES

## Memory:

```
"custom-graph/memory": {
    "interval": 60,
    "graph_type": "gauge",
    "width": 52,
    "exec": "/path/mem.sh",
    "signal": 8,
    "return-type": "json"
},
```

mem.sh:

```
#!/bin/bash

mem_info=$(cat /proc/meminfo)
mem_total=$(echo "$mem_info" | grep '^MemTotal:' | awk '{print $2}')
mem_available=$(echo "$mem_info" | grep '^MemAvailable:' | awk '{print $2}')

mem_used=$((mem_total - mem_available))
mem_percent=$((mem_used * 100 / mem_total))

echo "{\"text\": \"${mem_percent}%\", \"percentage\": ${mem_percent},\"tooltip\": \"Memory: ${mem_used}KB used / ${mem_total}KB total\"}'"
```

# STYLE

- *#custom-graph-<name>*
- *#custom-graph-<name>.<class>*
- *<class>* can be set by the script. For more information see *return-type*
