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
	Explicit order values for items in the tray. ++
	Key: name of item. ++
	Value: integer, higher value means to place this item to the right. ++
	Items without an entry default to 0, so a negative value places an item
	left of all unconfigured ones and a positive value places it right of
	them.

# ITEM KEYS AND DEFAULT ORDER

An item is addressed by its key in *orders* and *icons*. The key is the
item's SNI *Id*, except for Chrome-based applications, whose *Id* is not
unique -- those are addressed by their tooltip text, lowercased.

Since neither is visible anywhere in the UI, waybar logs the key of every
item it registers on startup:

```
[info] tray: item key 'nm-applet'
[info] tray: item key 'Rocket.Chat_status_icon_1'
```

Run waybar from a terminal to see them.

Items that no entry in *orders* applies to are placed in alphabetical
order of their key, after those with a negative order value and before
those with a positive one. Ordering therefore does not depend on the
order in which applications register on D-Bus, which is racy and differs
between restarts and between outputs of the same bar.

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
	}
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
