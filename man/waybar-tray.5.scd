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
	default: false ++
	Mirrors the resulting icon order.

*order-left*: ++
	typeof: array ++
	List of tray item keys that should be pinned to the left (or top,
	for vertical bars), in the given order. Keys are matched
	case-insensitively against the item's sort key, which is its SNI
	*Id* (or the tooltip text up to the first ':', lowercased, for
	Chrome-based apps). The key for each registered item is printed on
	startup as an info log line of the form _tray: item key='...'_ —
	run waybar from a terminal to see it.

*order-right*: ++
	typeof: array ++
	Same as *order-left*, but pinned to the right (or bottom, for
	vertical bars). ++
	Any item listed in neither *order-left* nor *order-right* nor
	*orders* is placed between the two groups in alphabetical order,
	so the tray layout does not depend on the order in which the
	applications happen to register.

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
	Explicit order values for items in the tray. ++
	Key: name of item. ++
	Value: integer, higher value means to place this item to the right. ++
	Takes precedence over *order-left* / *order-right*. Items with an equal
	order value are sorted alphabetically.

Note: custom *icons* only work with actual image files, not font-based icons. They
might not work for some Electron apps (see https://github.com/electron/electron/issues/40936).

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
	},
	"order-left": ["nm-applet", "blueman"],
	"order-right": ["TelegramDesktop", "vesktop"]
}

```

# STYLE

- *#tray*
- *#tray menu* for the context menu
- *#tray widget>image* for icons
- *#tray > .passive* for icons with status *Passive*
- *#tray > .active* for icons with status *Active*
- *#tray > .needs-attention* for icons with status *NeedsAttention*

# TRAY APPLETS

The following applets are known to work with the *tray* module:

- *NetworkManager*: install networkmanager, enable it with
  *systemctl enable NetworkManager.service*, and start the applet with
  *nm-applet --indicator*.
- *Blueman*: enable the Bluetooth daemon, install blueman, and start
  *blueman-applet*.
- *Electron apps*: apps like Slack, Telegram, Caprine and Discord should work
  out of the box.
