waybar-power-profiles-daemon(5)

# NAME

waybar - power-profiles-daemon module

# DESCRIPTION

The *power-profiles-daemon* module displays the active power-profiles-daemon profile and cycle through the available profiles on click.

# FILES

$XDG_CONFIG_HOME/waybar/config

# CONFIGURATION


[- *Option*
:- *Typeof*
:- *Default*
:= *Description*
|[ *format*
:[ string
:[ "{icon}"
:[ Message displayed on the bar. {icon} and {profile} are respectively substituted with the icon representing the active profile and its full name.
|[ *tooltip-format*
:[ string
:[ "Power profile: {profile}\\nDriver: {driver}"
:[ Messaged displayed in the module tooltip. {icon} and {profile} are respectively substituted with the icon representing the active profile and its full name.
|[ *tooltip*
:[ bool
:[ true
:[ Display the tooltip.
|[ *format-icons*
:[ object
:[ See default value in the example below.
:[ Icons used to represent the various power-profile. *Note*: the default configuration uses the font-awesome icons. You may want to override it if you don't have this font installed on your system.
|[ *expand*:
:[ bool
:[ false
:[ Enables this module to consume all left over space dynamically.





# CONFIGURATION EXAMPLES

Compact display (default config):

```
"power-profiles-daemon": {
  "format": "{icon}",
  "tooltip-format": "Power profile: {profile}\nDriver: {driver}",
  "tooltip": true,
  "format-icons": {
    "default": "",
    "performance": "",
    "balanced": "",
    "power-saver": ""
  }
}
```

Display the full profile name:

```
"power-profiles-daemon": {
  "format": "{icon}   {profile}",
  "tooltip-format": "Power profile: {profile}\nDriver: {driver}",
  "tooltip": true,
  "format-icons": {
    "default": "",
    "performance": "",
    "balanced": "",
    "power-saver": ""
  }
}
```
