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

*icons*: ++
	typeof: object ++
	Per-application icon customization. Keys are application IDs (e.g., "spotify", "blueman"). ++
	Values can be: ++
		- *string*: Custom icon name or path to icon file ++
		- *false*: Hide this application's tray icon completely

*orders*: ++
	typeof: object ++
	Orders for items to be placed in tray. When not set, orders are controlled by DBus. ++
	Key: name of item. ++
	Value: integer, higher value means to place this item to the right.

*ignore-list*: ++
	typeof: array ++
	List of strings used to hide matching tray items. An item is hidden when any string in the list is found (as a substring) in the item's bus name, category, icon name, id or title.

# EXAMPLES

```
"tray": {
	"icon-size": 21,
	"spacing": 10,
	"icons": {
		"blueman": "bluetooth",
		"TelegramDesktop": "$HOME/.local/share/icons/hicolor/16x16/apps/telegram.png",
		"spotify": false
	},
	"orders": {
		"wechat": 99
	}
}

```

# STYLE

- *#tray*
- *#tray > .passive*
- *#tray > .active*
- *#tray > .needs-attention*
