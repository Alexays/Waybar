waybar-wwan(5) "waybar-wwan" "User Manual"

# NAME

waybar - wwan module

# DESCRIPTION

*wwan* module for ModemManager.


# FILES

$XDG_CONFIG_HOME/waybar/config ++
	Per user configuration file

# ADDITIONAL FILES

libmm-glib lives in:

. /usr/lib/libmm-glib.so or /usr/lib64/libmm-glib.so
. /usr/lib/pkgconfig/libmm-glib.pc or /usr/lib64/pkgconfig/libmm-glib.pc
. /usr/include/libmm-glib

# CONFIGURATION

*format*: ++
	typeof: string  ++
	default: *{state}* ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-failed*: ++
	typeof: string ++
	This format is used when the modem cannot start.

*format-locked*: ++
	typeof: string ++
	This format is used when the SIM on the modem is locked.

*format-disabled*: ++
	typeof: string ++
	This format is used when the modem is disabled.

*format-enabled*: ++
	typeof: string ++
	This format is used when the modem is enabled, but isn't registered to any carrier network.

*format-searching*: ++
	typeof: string ++
	This format is used when the modem is looking for a carrier network to register to.

*format-registered*: ++
	typeof: string ++
	This format is used when the modem is registered to a network, but no data connection is active.

*format-connected*: ++
	typeof: string ++
	This format is used when the modem is registered to a network, and there is an active data connection.

*format-icons*: ++
	typeof: array/object ++
	Based on the current signal strength, the corresponding icon gets selected. ++
	The order is *low* to *high*. Or by the state if it is an object.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: strirng ++
	default: {state} ++
	The format, how information should be displayed in the tooltip. This format is used when other formats aren't specified.

*tooltip-format-failed*: ++
	typeof: string ++
	This format is used when the modem cannot start.

*tooltip-format-locked*: ++
	typeof: string ++
	This format is used when the SIM on the modem is locked.

*tooltip-format-disabled*: ++
	typeof: string ++
	This format is used when the modem is disabled.

*tooltip-format-enabled*: ++
	typeof: string ++
	This format is used when the modem is enabled, but isn't registered to any carrier network.

*tooltip-format-searching*: ++
	typeof: string ++
	This format is used when the modem is looking for a carrier network to register to.

*tooltip-format-registered*: ++
	typeof: string ++
	This format is used when the modem is registered to a network, but no data connection is active.

*tooltip-format-connected*: ++
	typeof: string ++
	This format is used when the modem is registered to a network, and there is an active data connection.

*interval*: ++
	typeof: integer ++
	default: 5 ++
	The interval in which the WWAN information gets polled (e.g. carrier name).

*imei*: ++
	typeof: string ++
	Select only the modem with the corresponding imei.

*path*: ++
	typeof: string ++
	Select only the modem with the corresponding hardware path.

*hide-failed*: ++
	typeof: bool ++
	default: true ++
	Defines if the module should be hidden if the modem cannot start.

*hide-disabled*: ++
	typeof: bool ++
	default: false ++
	Defines if the module should be hidden if the modem is disabled.

*hide-not-registered*: ++
	typeof: bool ++
	default: false ++
	Defines if the module should be hidden if the modem isn't registered with any carrier network.


# FORMAT REPLACEMENTS

*{state}*: Modem state.

*{current_modes}*: List of all the currently active connection modes.

*{preferred_mode}*: The preferred connection mode in use.

*{signal_quality}*: Signal quality (0-100).

*{operator_name}*: Name of the network operator.

# EXAMPLES

```
"wwan": {
	"format": "{state}",
	"format-disabled": "", // an empty format will hide the module
	"format-locked": "SIM Locked",
    "format-connected": "{operator_name} {preferred_mode} | {signal_quality}%"
	"tooltip-format": "{mode}",
	"tooltip-format-connected": "{current_modes}",
	"hide-disconnected": false
}
```
# STYLE

- *#wwan*
- *#wwan.failed* Applied when the modem cannot start.
- *#wwan.locked* Applied when the SIM on the modem is locked.
- *#wwan.disabled* Applied when the modem is disabled.
- *#wwan.enabled* Applied when the modem is enabled, but isn't registered to any carrier network.
- *#wwan.searching* Applied when the modem is looking for a carrier network to register to.
- *#wwan.registered* Applied when the modem is registered to a network, but no data connection is active.
- *#wwan.connected* Applied when the modem is registered to a network, and there is an active data connection.
