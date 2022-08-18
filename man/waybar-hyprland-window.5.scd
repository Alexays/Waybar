waybar-hyprland-window(5)

# NAME

waybar - hyprland window module

# DESCRIPTION

The *window* module displays the title of the currently focused window in Hyprland.

# CONFIGURATION

Addressed by *hyprland/window*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} the current window title is displayed.


# EXAMPLES

```
"hyprland/window": {
    "format": "{}"
}
```

# STYLE

- *#window*
