waybar-sway-window(5)

# NAME

waybar - sway window module

# DESCRIPTION

The *window* module displays the title of the currently focused window in Sway

# CONFIGURATION

Addressed by *sway/window*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} data gets inserted.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# EXAMPLES

```
"sway/window": {
    "format": "{}",
    "max-length": 50
}
```

# STYLE

- *#window*
- *window#waybar.empty* When no windows is in the workspace
- *window#waybar.solo* When one window is in the workspace
- *window#waybar.<app_id>* Where *app_id* is the app_id or *instance* name like (*chromium*) of the only window in the workspace
