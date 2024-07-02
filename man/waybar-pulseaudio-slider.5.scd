waybar-pulseaudio-slider(5)

# NAME

waybar - pulseaudio slider module

# DESCRIPTION

The *pulseaudio slider* module displays and controls the current volume of the default sink or source as a bar.

The volume can be controlled by dragging the slider across the bar or clicking on a specific position.

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

# EXAMPLES

```
"modules-right": [
    "pulseaudio/slider",
],
"pulseaudio/slider": {
    "min": 0,
    "max": 100,
    "orientation": "horizontal"
}
```

# STYLE

The slider is a component with multiple CSS Nodes, of which the following are exposed:

*#pulseaudio-slider*: ++
    Controls the style of the box *around* the slider and bar.

*#pulseaudio-slider slider*: ++
    Controls the style of the slider handle.

*#pulseaudio-slider trough*: ++
    Controls the style of the part of the bar that has not been filled.

*#pulseaudio-slider highlight*: ++
    Controls the style of the part of the bar that has been filled.

## STYLE EXAMPLE

```
#pulseaudio-slider slider {
    min-height: 0px;
    min-width: 0px;
    opacity: 0;
    background-image: none;
    border: none;
    box-shadow: none;
}

#pulseaudio-slider trough {
    min-height: 80px;
    min-width: 10px;
    border-radius: 5px;
    background-color: black;
}

#pulseaudio-slider highlight {
    min-width: 10px;
    border-radius: 5px;
    background-color: green;
}
```
