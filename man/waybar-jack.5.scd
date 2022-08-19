waybar-jack(5)

# NAME

waybar - JACK module

# DESCRIPTION

The *jack* module displays the current state of the JACK server.

# CONFIGURATION

Addressed by *jack*

*format*: ++
    typeof: string ++
    default: *{load}%* ++
    The format, how information should be displayed. This format is used when other formats aren't specified.

*format-connected*: ++
    typeof: string ++
    This format is used when the module is connected to the JACK server.

*format-disconnected*: ++
    typeof: string ++
    This format is used when the module is not connected to the JACK server.

*format-xrun*: ++
    typeof: string ++
    This format is used for one polling interval, when the JACK server reports an xrun.

*realtime*: ++
    typeof: bool ++
    default: *true* ++
    Option to drop real-time privileges for the JACK client opened by Waybar.

*tooltip*: ++
    typeof: bool ++
    default: *true* ++
    Option to disable tooltip on hover.

*tooltip-format*: ++
    typeof: string ++
    default: *{bufsize}/{samplerate} {latency}ms* ++
    The format of information displayed in the tooltip.

*interval*: ++
    typeof: integer ++
    default: 1 ++
    The interval in which the information gets polled.

*rotate*: ++
    typeof: integer ++
    Positive value to rotate the text label.

*max-length*: ++
    typeof: integer ++
    The maximum length in character the module should display.

*min-length*: ++
    typeof: integer ++
    The minimum length in characters the module should take up.

*align*: ++
    typeof: float ++
    The alignment of the text, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*on-click*: ++
    typeof: string ++
    Command to execute when clicked on the module.

*on-click-middle*: ++
    typeof: string ++
    Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
    typeof: string ++
    Command to execute when you right clicked on the module.

*on-update*: ++
    typeof: string ++
    Command to execute when the module is updated.

# FORMAT REPLACEMENTS

*{load}*: The current CPU load estimated by JACK.

*{bufsize}*: The size of the JACK buffer.

*{samplerate}*: The samplerate at which the JACK server is running.

*{latency}*: The duration, in ms, of the current buffer size.

*{xruns}*: The number of xruns reported by the JACK server since starting Waybar.

# EXAMPLES

```
"jack": {
    "format": "DSP {}%",
    "format-xrun": "{xruns} xruns",
    "format-disconnected": "DSP off",
    "realtime": true
}
```

# STYLE

- *#jack*
- *#jack.connected*
- *#jack.disconnected*
- *#jack.xrun*
