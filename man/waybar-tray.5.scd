waybar-tray(5)

# NAME

waybar - tray module

# DESCRIPTION

_WARNING_ *tray* is still in beta. There may me bugs. Breaking changes may occur.

# CONFIGURATION

Addressed by *tray*

*icon-size*: ++
    typeof: integer ++
    Defines the size of the tray icons.

*spacing*: ++
    typeof: integer ++
    Defines the spacing between the tray icons.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

# EXAMPLES

```
"tray": {
    "icon-size": 21,
    "spacing": 10
}

```

# STYLE

- *#tray*
