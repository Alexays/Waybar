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
. /usr/lib/pkgconfig/libgps.pc or /usr/lib64/pkgconfig/libgps.pc
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
	typeof: string ++
	default: Games running: {glyph} ++
	The text format of the tooltip.

*interval*: ++
	typeof: integer ++
	default: 5 ++
	The interval in which the WWAN information gets polled (e.g. carrier name).

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

*{mode}*: Fix mode

*{status}*: Technology used for GPS fix. Not all GPS receivers report this.

*{latitude}*: Latitude, decimal degrees. Can be NaN.

*{latitude_error}*: Latitude uncertainty, meters. Can be NaN.

*{longitude}*: Longitude, decimal degrees. Can be NaN.

*{longitude_error}*: Longitude uncertainty, meters. Can be NaN.

*{altitude_hae}*: Altitude, height above ellipsoid, meters. Can be NaN.

*{altitude_msl}*: Longitude, MSL, meters. Can be NaN.

*{altitude_error}*: Altitude uncertainty, meters. Can be NaN.

*{speed}*: Speed over ground, meters/sec. Can be NaN.

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
