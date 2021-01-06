waybar-sway-language(5)

# NAME

waybar - sway language module

# DESCRIPTION

The *language* module displays the current keyboard layout in Sway

# CONFIGURATION

Addressed by *sway/language*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} data gets inserted.

# EXAMPLES

```
"sway/language": {
    "format": "{}",
    "max-length": 50
}
```

# STYLE

- *#language*
