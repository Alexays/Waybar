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



# EXAMPLES

```
"gps": {

},
```
# STYLE

- *#gps*
- *#gps.fix-none* Applied when GPS is present, but there is no fix.
- *#gps.fix-2d* Applied when there is a 2D fix.
- *#gps.fix-3d* Applied when there is a 3D fix.
