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
	Reverses the alphabetical ordering of icons in the middle section
	(icons not pinned via *order-left* or *order-right*). Items pinned
	via *order-left* / *order-right* are unaffected and keep their
	configured visual position.

*order-left*: ++
	typeof: array ++
	List of tray item keys that should be pinned to the left (or top,
	for vertical bars), in the given order. Keys are matched
	case-insensitively against the item's sort key, which is its SNI
	*Id* (or the tooltip text, lowercased, for Chrome-based apps).
	The key for each registered item is printed on startup as an info
	log line of the form _tray: item key='...'_ — run waybar from a
	terminal to see it.

*order-right*: ++
	typeof: array ++
	List of tray item keys that should be pinned to the right (or
	bottom, for vertical bars), in the given order. Any item not
	listed in *order-left* or *order-right* is placed between the two
	groups in alphabetical order.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# EXAMPLES

```
"tray": {
	"icon-size": 21,
	"spacing": 10,
  "icons": {
    "blueman": "bluetooth",
    "TelegramDesktop": "$HOME/.local/share/icons/hicolor/16x16/apps/telegram.png"
  },
  "order-left": ["nm-applet", "blueman"],
  "order-right": ["TelegramDesktop", "vesktop"]
}

```

# STYLE

- *#tray*
- *#tray > .passive*
- *#tray > .active*
- *#tray > .needs-attention*
