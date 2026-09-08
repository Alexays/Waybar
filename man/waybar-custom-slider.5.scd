waybar-custom-slider(5)

# NAME

waybar - custom slider module

# DESCRIPTION

The *custom-slider* module displays and controls a value from an external
command as a slider bar. The value is *read* from the *exec* command and
*written* by the *on-change* command, so any target with a CLI -- *brightnessctl*,
*ddcutil*, *wpctl*, *hyprctl hyprsunset gamma*, a shell script -- can be driven
with no Waybar-side code.

The value can be controlled by dragging the slider across the bar, clicking on a
position, scrolling, or with the arrow keys when focused.

# CONFIGURATION

Addressed by *custom-slider/<name>*

*exec*: ++
	typeof: string ++
	The read command. Its first line of output is the current value; a trailing
	*%* is stripped. With *return-type* *json*, the *percentage* field is used
	instead. If unset, the slider is write-only.

*exec-if*: ++
	typeof: string ++
	A gate command; *exec* runs only when *exec-if* exits 0.

*on-change*: ++
	typeof: string ++
	The write command, run when the value is committed. *{}* is replaced with the
	integer value, e.g. `hyprctl hyprsunset gamma {}`. It is the only interpolation;
	the command itself is user-supplied (same trust model as *custom*).

*interval*: ++
	typeof: integer or float ++
	The interval in seconds at which *exec* is polled. Use *once* to read a single
	time. If no *interval* or *signal* is set, *exec* is assumed to loop and stream
	values itself.

*signal*: ++
	typeof: integer ++
	Update on *SIGRTMIN+N*. Pair it with a keybind (e.g.
	`hyprctl hyprsunset gamma +10 && pkill -RTMIN+8 waybar`) for instant sync when
	the value is changed outside Waybar.

*restart-interval*: ++
	typeof: integer or float ++
	For a continuous *exec*, re-run it this many seconds after it exits. Cannot be
	used with *interval*.

*return-type*: ++
	typeof: string ++
	*json* reads the value from the *percentage* field of a JSON object (also
	*tooltip* and *class*). Anything else reads the raw first line.

*value-regex*: ++
	typeof: string ++
	Optional. A regular expression whose capture group 1 is the value, for output
	the default heuristic (strip a trailing *%*, parse the first number) cannot
	handle. Group 1 must be numeric; an invalid pattern is ignored with a warning.

*write-behaviour*: ++
	typeof: string ++
	default: on-release ++
	When a value is written as you interact. ++
	*on-release* writes once, when a drag ends; a keyboard step or scroll writes immediately. ++
	*throttled* writes continuously during a drag, at most once per *write-interval*. ++
	*debounced* writes only after movement pauses for *write-interval*.

*write-interval*: ++
	typeof: uint ++
	default: 40 ++
	Minimum milliseconds between writes for *throttled* and *debounced*. Ignored
	under *on-release*. For an *exec* write command, 100 is a gentler choice.

*failure-behaviour*: ++
	typeof: string ++
	default: keep ++
	What to do when a read fails or its output cannot be parsed. A failed read is
	*never* written back, and *never* becomes 0. ++
	*keep* retains the last value and adds the *.stale* CSS class. ++
	*min* snaps the handle to *min*. ++
	*hide* hides the slider until a read succeeds.

*min*: ++
	typeof: int ++
	default: 0 ++
	The minimum value the slider displays and sets.

*max*: ++
	typeof: int ++
	default: 100 ++
	The maximum value the slider displays and sets.

*orientation*: ++
	typeof: string ++
	default: horizontal ++
	The orientation of the slider, either *horizontal* or *vertical*.

*escape*: ++
	typeof: bool ++
	default: false ++
	Escape markup in the text and tooltip read from *exec*.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Show the read output (or the JSON *tooltip* field) as a hover tooltip.

*Note*: Like all slider modules, this one *requires* styling to render: you *need*
to set *min-width* and/or *min-height* (depending on orientation) on the *trough*
node. That is a GTK detail, not a Waybar issue. See the *STYLE* section.

# EXAMPLES

```
"modules-right": [
    "custom-slider/brightness",
],
"custom-slider/brightness": {
    "exec": "hyprctl hyprsunset gamma",
    "on-change": "hyprctl hyprsunset gamma {}",
    "interval": 5,
    "signal": 8,
    "min": 10,
    "max": 200,
    "orientation": "horizontal"
}
```

# STYLE

The slider is a component with multiple CSS Nodes, of which the following are exposed:

*#custom-slider-<name>*: ++
	Controls the style of the box *around* the slider and bar.

*#custom-slider-<name>.stale*: ++
	Controls the style when the last read failed (see *failure-behaviour* *keep*).

*#custom-slider-<name> slider*: ++
	Controls the style of the slider handle.

*#custom-slider-<name> trough*: ++
	Controls the style of the part of the bar that has not been filled.

*#custom-slider-<name> highlight*: ++
	Controls the style of the part of the bar that has been filled.

## STYLE EXAMPLE

```
#custom-slider-brightness slider {
    min-height: 0px;
    min-width: 0px;
    opacity: 0;
    background-image: none;
    border: none;
    box-shadow: none;
}

#custom-slider-brightness trough {
    min-height: 80px;
    min-width: 10px;
    border-radius: 5px;
    background-color: black;
}

#custom-slider-brightness highlight {
    min-width: 10px;
    border-radius: 5px;
    background-color: yellow;
}

#custom-slider-brightness.stale trough {
    background-color: #444;
}
```
