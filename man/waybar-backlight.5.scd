waybar-backlight(5)

# NAME

waybar - backlight module

# DESCRIPTION

The *backlight* module displays the current backlight level.

# CONFIGURATION

*interval*: ++
	typeof: integer ++
	default: 2 ++
	The interval in which information gets polled.

*format*: ++
	typeof: string ++
	default: {percent}% ++
	The format, how information should be displayed. On {} data gets inserted.

*states*: ++
	typeof: array ++
	A number of backlight states which get activated on certain brightness levels.

# EXAMPLE:

```
"backlight": {
    "device": "intel_backlight",
    "format": "{percent}% {icon}",
    "format-icons": ["", ""]
}
```

# STYLE

- *#backlight*
