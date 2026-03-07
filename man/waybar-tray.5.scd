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

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

*order*: ++
	typeof: array ++
	A list of tray item keys in the desired order. ++
	Items listed here are ordered according to this list. ++
	Items not present in the list are ordered alphabetically by their key. ++
	Matching is performed case-insensitively. ++
	The values must match the key printed in the log line:

	tray: item key='<KEY>' ...

*order-unknown*: ++
	typeof: string ++
	default: after ++
	Controls where items not present in *order* are placed.

	*after*  – configured items come first, unconfigured items follow. ++
	*before* – unconfigured items come first, configured items follow.

# EXAMPLES

```
"tray": {
	"icon-size": 21,
	"spacing": 10,
  "icons": {
    "blueman": "bluetooth",
    "TelegramDesktop": "$HOME/.local/share/icons/hicolor/16x16/apps/telegram.png"
  },
	"order": [
		"TelegramDesktop",
		"signal desktop",
	],
	"order-unknown": "after",
}

```

# STYLE

- *#tray*
- *#tray > .passive*
- *#tray > .active*
- *#tray > .needs-attention*
