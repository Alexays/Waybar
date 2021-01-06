waybar-sway-mode(5)

# NAME

waybar - sway mode module

# DESCRIPTION

The *mode* module displays the current binding mode of Sway

# CONFIGURATION

Addressed by *sway/mode*

*format*: ++
	typeof: string  ++
	default: {} ++
	The format, how information should be displayed. On {} data gets inserted.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# EXAMPLES

```
"sway/window": {
    "format": " {}",
    "max-length": 50
}
```

# STYLE

- *#mode*
