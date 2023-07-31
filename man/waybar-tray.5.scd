waybar-tray(5)

# NAME

waybar - tray module

# DESCRIPTION

_WARNING_ *tray* is still in beta. There may be bugs. Breaking changes may occur.

# CONFIGURATION

Addressed by *tray*

*icon-size*: ++
	typeof: integer ++
	Defines the size of the tray icons.

*show-passive-items*: ++
	typeof: bool ++
	default: false ++
	Defines visibility of the tray icons with *Passive* status.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

*spacing*: ++
	typeof: integer ++
	Defines the spacing between the tray icons.

*reverse-direction*: ++
	typeof: bool ++
	Defines if new app icons should be added in a reverse order

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
- *#tray > .passive*
- *#tray > .active*
- *#tray > .needs-attention*
