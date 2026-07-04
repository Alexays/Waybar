waybar-backlight-slider(5)

# NAME

waybar - backlight slider module

# DESCRIPTION

The *backlight slider* module displays and controls the current brightness of the default or preferred device.

The brightness can be controlled by dragging the slider across the bar or clicking on a specific position.

# CONFIGURATION

*min*: ++
    typeof: int ++
    default: 0 ++
    The minimum volume value the slider should display and set.

*max*: ++
    typeof: int ++
    default: 100 ++
    The maximum volume value the slider should display and set.

*orientation*: ++
    typeof: string ++
    default: horizontal ++
    The orientation of the slider. Can be either `horizontal` or `vertical`.

*device*: ++
    typeof: string ++
    The name of the preferred device to control. If left empty, a device will be chosen automatically.

*interval*: ++
    typeof: uint ++
    default: 1000 ++
    The interval in milliseconds at which the brightness is polled and the slider is updated.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

*Note*: As well as the JSON configuration, the slider modules are special in
that they *require* styling to work. You *need* to set *min-width* and/or
*min-height* (depending on whether your slider is vertical or not) for it to
display correctly. That is a GTK detail, not an issue with Waybar. See the
*STYLE* section below.

*Warning*: If *min* is set to *0* (default), the slider can set brightness to
*0*, which may completely disable the backlight on some devices, making the
screen fully black. Consider setting a small minimum value (e.g. *10*) or
configuring brightness keybinds as a fallback.

# EXAMPLES

```
"modules-right": [
    "backlight/slider",
],
"backlight/slider": {
    "min": 0,
    "max": 100,
    "orientation": "horizontal",
    "device": "intel_backlight"
}
```

# STYLE

The slider is a component with multiple CSS Nodes, of which the following are exposed:

*#backlight-slider*: ++
    Controls the style of the box *around* the slider and bar.

*#backlight-slider slider*: ++
    Controls the style of the slider handle.

*#backlight-slider trough*: ++
    Controls the style of the part of the bar that has not been filled.

*#backlight-slider highlight*: ++
    Controls the style of the part of the bar that has been filled.

## STYLE EXAMPLE

```
#backlight-slider slider {
    min-height: 0px;
    min-width: 0px;
    opacity: 0;
    background-image: none;
    border: none;
    box-shadow: none;
}

#backlight-slider trough {
    min-height: 80px;
    min-width: 10px;
    border-radius: 5px;
    background-color: black;
}

#backlight-slider highlight {
    min-width: 10px;
    border-radius: 5px;
    background-color: red;
}
```
