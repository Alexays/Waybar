waybar-keyboard-state(5)

# NAME

waybar - keyboard-state module

# DESCRIPTION

The *keyboard-state* module displays the state of number lock, caps lock, and scroll lock.

You must be a member of the input group to use this module.

# CONFIGURATION

*interval*: ++
	Deprecated, this module uses event loop now, the interval has no effect.
	typeof: integer ++
	default: 1 ++
	The interval, in seconds, to poll the keyboard state.

*format*: ++
	typeof: string|object ++
	default: {name} {icon} ++
	The format, how information should be displayed. If a string, the same format is used for all keyboard states. If an object, the fields "numlock", "capslock", and "scrolllock" each specify the format for the corresponding state. Any unspecified states use the default format.

*format-icons*: ++
	typeof: object ++
	default: {"locked": "locked", "unlocked": "unlocked"} ++
	Based on the keyboard state, the corresponding icon gets selected. Supports two syntaxes:
	- Common format-icons: "locked" and "unlocked" keys apply to all lock types.
	- Per-lock-type format-icons: per-lock-type objects with "numlock", "capslock", "scrolllock" keys, each containing "locked" and "unlocked" icons.
	See *icons*.

*numlock*: ++
	typeof: bool ++
	default: false ++
	Display the number lock state.

*capslock*: ++
	typeof: bool ++
	default: false ++
	Display the caps lock state.

*scrolllock*: ++
	typeof: bool ++
	default: false ++
	Display the scroll lock state.

*device-path*: ++
	typeof: string ++
	default: chooses first valid input device ++
	Which libevdev input device to show the state of. Libevdev devices can be found in /dev/input. The device should support number lock, caps lock, and scroll lock events.

*binding-keys*: ++
	typeof: array ++
	default: [58, 69, 70] ++
	Customize the key to trigger this module, the key number can be found in /usr/include/linux/input-event-codes.h or running sudo libinput debug-events --show-keycodes.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# FORMAT REPLACEMENTS

*{name}*: Caps, Num, or Scroll.

*{icon}*: Icon, as defined in *format-icons*.

# ICONS

The following *format-icons* can be set.

## Common format-icons for all lock types:
- *locked*: Will be shown when the keyboard state is locked. Default "locked".
- *unlocked*: Will be shown when the keyboard state is not locked. Default "unlocked".

## Per-lock-type format-icons:
- *numlock*, *capslock*, *scrolllock*: Object containing "locked" and "unlocked" keys for lock-type-specific icons. Defaults to {"locked": "locked", "unlocked": "unlocked"} for each lock type.

# EXAMPLE:

## Common format-icons for all lock types:
```
"keyboard-state": {
	"numlock": true,
	"capslock": true,
	"scrolllock": true,
	"format": "{name} {icon}",
	"format-icons": {
		"locked": "",
		"unlocked": ""
	}
}
```

## Per-lock-type format-icons:
```
"keyboard-state": {
	"numlock": true,
	"capslock": true,
	"scrolllock": true,
	"format": "{name} {icon}",
	"format-icons": {
		"numlock": {
			"locked": "1",
			"unlocked": "0"
		},
		"capslock": {
			"locked": "A",
			"unlocked": "a"
		},
		"scrolllock": {
			"locked": "S",
			"unlocked": "s"
		}
	}
}
```

# STYLE

- *#keyboard-state*
- *#keyboard-state label*
- *#keyboard-state label.locked*
- *#keyboard-state label.numlock*
- *#keyboard-state label.numlock.locked*
- *#keyboard-state label.capslock*
- *#keyboard-state label.capslock.locked*
- *#keyboard-state label.scrolllock*
- *#keyboard-state label.scrolllock.locked*
