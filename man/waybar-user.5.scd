waybar-user(5) "waybar-user" "User Manual"

# NAME

waybar - user module

# DESCRIPTION

The *user* module displays the currently logged in user, the system uptime and,
optionally, the user's avatar image.

# CONFIGURATION

Addressed by *user*

[- *Option*
:- *Typeof*
:- *Default*
:- *Description*
|[ *interval*
:[ integer
:[ 60
:[ The interval in which the information gets polled
|[ *format*
:[ string
:[ *{user} {work_H}:{work_M}*
:[ The format, how the information should be displayed. See *FORMAT REPLACEMENTS* below
|[ *icon*
:[ bool
:[ false
:[ When enabled, displays the user's avatar image next to the label
|[ *avatar*
:[ string
:[ *$HOME/.face*
:[ Path to the avatar image to display. Only used when *icon* is enabled. When unset, the default path *$HOME/.face* is used
|[ *height*
:[ integer
:[ 20
:[ The height of the avatar image in pixels
|[ *width*
:[ integer
:[ 20
:[ The width of the avatar image in pixels
|[ *open-on-click*
:[ bool
:[ false
:[ When enabled, left-clicking the module opens the user's home directory (or *open-path*) with the default file manager
|[ *open-path*
:[ string
:[
:[ Overrides the path that is opened when *open-on-click* is enabled. When empty, the user's home directory is used
|[ *rotate*
:[ integer
:[
:[ Positive value to rotate the text label (in 90 degree increments)
|[ *tooltip*
:[ bool
:[ true
:[ Option to enable tooltip on hover

# FORMAT REPLACEMENTS

- *{user}*: The login name of the currently logged in user, in uppercase
- *{work_d}*: Whole days the system has been up
- *{work_H}*: Hours part of the system uptime (00-23)
- *{work_M}*: Minutes part of the system uptime (00-59)
- *{work_S}*: Seconds part of the system uptime (00-59)
- *{up_H}*: Hour the system was booted (00-23)
- *{up_M}*: Minute the system was booted (00-59)
- *{up_d}*: Day of the month the system was booted (01-31)
- *{up_m}*: Month the system was booted (01-12)
- *{up_Y}*: Year the system was booted

# EXAMPLES

```
"user": {
	"format": "{user} up {work_H}:{work_M}:{work_S}",
	"interval": 60,
	"icon": true,
	"avatar": "/home/alice/.face",
	"height": 24,
	"width": 24,
	"open-on-click": true
}
```

# STYLE

- *#user*

# AUTHOR

Alexis Rouillard <contact@arouillard.fr>
