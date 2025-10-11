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
	typeof: string ++
	default: {glyph} ++
	The text format.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: strirng ++
	default: Games running: {glyph} ++
	The text format of the tooltip.

*interval*: ++
	typeof: integer ++
	default: 5 ++
	The interval in which the WWAN information gets polled (e.g. carrier name).

*imei*: ++
	typeof: string ++
	default: {glyph} ++
	Select only the modem with the corresponding imei.

*path*: ++
	typeof: string ++
	default: {glyph} ++
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

*{access_technologies}*: List of all the supported access technologies.

*{current_modes}*: List of all the currently active connection modes.

*{preferred_mode}*: The preferred connection mode in use.

*{supported_modes}*: List of all the supported connection modes.

*{current_bands}*: List of all the currently active frequency bands.

*{supported_bands}*: List of all the supported frequency bands.

*{signal_quality}*: Signal quality (0-100).

*{operator_name}*: Name of the network operator.

*{speed_error}*: Speed uncertainty, meters/sec. Can be NaN.

*{climb}*: Vertical speed, meters/sec. Can be NaN.

*{climb_error}*: Vertical speed uncertainty, meters/sec. Can be NaN.

*{satellites_visible}*: Number of satellites visible from the GPS receiver.

*{satellites_used}*: Number of satellites used for the GPS fix.

# EXAMPLES

```
"wwan": {
	"format": "{state}",
	"format-disabled": "", // an empty format will hide the module
	"format-locked": "SIM Locked",
    "format-connected": "{operator_name} {preferred_mode}"
	"tooltip-format": "{mode}",
	"tooltip-format-locked": "{satellites_visible} satellites visible",
	"tooltip-format-registered": "{satellites_used}/{satellites_visible} satellites used",
	"tooltip-format-connected": "",
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
