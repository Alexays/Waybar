waybar-gps(5) "waybar-gps" "User Manual"

# NAME

waybar - gps module

# DESCRIPTION

*gps* module for gpsd.


# FILES

$XDG_CONFIG_HOME/waybar/config ++
	Per user configuration file

# ADDITIONAL FILES

libgps lives in:

. /usr/lib/libgps.so or /usr/lib64/libgps.so
. /usr/lib/pkgconfig/libgps.pc or /usr/lib64/pkgconfig/libgps.pc
. /usr/include/gps

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
	The interval in which the GPS information gets polled (e.g. current speed).
	Significant updates (e.g. the current fix mode) are updated immediately.

*hide-disconnected*: ++
	typeof: bool ++
	default: true ++
	Defines if the module should be hidden if there is no GPS receiver.

*hide-no-fix*: ++
	typeof: bool ++
	default: false ++
	Defines if the module should be hidden if there is no GPS fix.

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
"gps": {
	"format": "{mode}",
	"format-disabled": "", // an empty format will hide the module
	"format-no-fix": "No fix",
	"format-fix-3d": "{status}",
	"tooltip-format": "{mode}",
	"tooltip-format-no-fix": "{satellites_visible} satellites visible",
	"tooltip-format-fix-2d": "{satellites_used}/{satellites_visible} satellites used",
	"tooltip-format-fix-3d": "Altitude: {altitude_hae}m",
	"hide-disconnected": false
}
```
# STYLE

- *#gps*
- *#gps.disabled* Applied when GPS is disabled.
- *#gps.fix-none* Applied when GPS is present, but there is no fix.
- *#gps.fix-2d* Applied when there is a 2D fix.
- *#gps.fix-3d* Applied when there is a 3D fix.
