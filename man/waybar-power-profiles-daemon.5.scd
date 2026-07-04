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
:[ "Power profile: {profile}\\nCPU driver: {cpu_driver}\\nPlatform driver: {platform_driver}"
:[ Messaged displayed in the module tooltip. {icon} and {profile} are respectively substituted with the icon representing the active profile and its full name. {cpu_driver} and {platform_driver} are substituted with the CPU and platform drivers reported by recent power-profiles-daemon versions. {driver} is kept for backward compatibility: it resolves to the legacy single driver on older daemons and falls back to the CPU driver on recent ones.
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





# FORMAT REPLACEMENTS

*{profile}*: Active profile full name.

*{icon}*: Icon (from *format-icons*) representing the active profile.

*{cpu_driver}*: The CPU driver reported by recent power-profiles-daemon versions. *(tooltip)*

*{platform_driver}*: The platform driver reported by recent power-profiles-daemon versions. *(tooltip)*

*{driver}*: Kept for backward compatibility: resolves to the legacy single driver on older daemons and falls back to the CPU driver on recent ones. *(tooltip)*

# CONFIGURATION EXAMPLES

Compact display (default config):

```
"power-profiles-daemon": {
  "format": "{icon}",
  "tooltip-format": "Power profile: {profile}\nCPU driver: {cpu_driver}\nPlatform driver: {platform_driver}",
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
  "tooltip-format": "Power profile: {profile}\nCPU driver: {cpu_driver}\nPlatform driver: {platform_driver}",
  "tooltip": true,
  "format-icons": {
    "default": "",
    "performance": "",
    "balanced": "",
    "power-saver": ""
  }
}
```

# STYLE

- *#power-profiles-daemon*
- *#power-profiles-daemon.performance* Applied when the *performance* profile is active.
- *#power-profiles-daemon.balanced* Applied when the *balanced* profile is active.
- *#power-profiles-daemon.power-saver* Applied when the *power-saver* profile is active.

*Note*: the module applies a CSS class equal to the name of the active profile, so any additional profile exposed by power-profiles-daemon can be styled by its name.
