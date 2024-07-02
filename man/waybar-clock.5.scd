waybar-clock(5) "waybar-clock" "User Manual"

# NAME

waybar - clock module

# DESCRIPTION

*clock* module displays current date and time

# FILES

$XDG_CONFIG_HOME/waybar/config ++
	Per user configuration file

# CONFIGURATION

1. Addressed by *clock*
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
:[ *{:%H:%M}*
:[ The format, how the date and time should be displayed. See format options below
|[ *timezone*
:[ string
:[
:[ The timezone to display the time in, e.g. America/New_York. "" represents
   the system's local timezone. See Wikipedia's unofficial list of timezones <https://en.wikipedia.org/wiki/List_of_tz_database_time_zones>
|[ *timezones*
:[ list of strings
:[
:[ A list of timezones (as in *timezone*) to use for time display, changed using
   the scroll wheel. Do not specify *timezone* option when *timezones* is specified.
   "" represents the system's local timezone
|[ *locale*
:[ string
:[
:[ A locale to be used to display the time. Intended to render times in custom
   timezones with the proper language and format
|[ *max-length*
:[ integer
:[
:[ The maximum length in character the module should display
|[ *rotate*
:[ integer
:[
:[ Positive value to rotate the text label
|[ *on-click*
:[ string
:[
:[ Command to execute when clicked on the module
|[ *on-click-middle*
:[ string
:[
:[ Command to execute when you middle clicked on the module using mousewheel
|[ *on-click-right*
:[ string
:[
:[ Command to execute when you right-click on the module
|[ *on-scroll-up*
:[ string
:[
:[ Command to execute when scrolling up on the module
|[ *on-scroll-down*
:[ string
:[
:[ Command to execute when scrolling down on the module
|[ *smooth-scrolling-threshold*
:[ double
:[
:[ Threshold to be used when scrolling
|[ *tooltip*
:[ bool
:[ true
:[ Option to enable tooltip on hover
|[ *tooltip-format*
:[ string
:[ same as format
:[ Tooltip on hover
|[ *menu*
:[ string
:[
:[ Action that popups the menu.
|[ *menu-file*
:[ string
:[
:[ Location of the menu descriptor file. There need to be an element of type GtkMenu with id *menu*
|[ *menu-actions*
:[ array
:[
:[ The actions corresponding to the buttons of the menu.

View all valid format options in *strftime(3)* or have a look https://en.cppreference.com/w/cpp/chrono/duration/formatter

2. Addressed by *clock: calendar*
[- *Option*
:- *Typeof*
:- *Default*
:- *Description*
|[ *mode*
:[ string
:[ month
:[ Calendar view mode. Possible values: year|month
|[ *mode-mon-col*
:[ integer
:[ 3
:[ Relevant for *mode=year*. Count of months per row
|[ *weeks-pos*
:[ integer
:[
:[ The position where week numbers should be displayed. Disabled when is empty.
   Possible values: left|right
|[ *on-scroll*
:[ integer
:[ 1
:[ Value to scroll months/years forward/backward. Can be negative. Is
   configured under *on-scroll* option

3. Addressed by *clock: calendar: format*
[- *Option*
:- *Typeof*
:- *Default*
:- *Description*
|[ *months*
:[ string
:[
:[ Format is applied to months header(January, February,...etc.)
|[ *days*
:[ string
:[
:[ Format is applied to days
|[ *weeks*
:[ string
:[ *{:%U}*
:[ Format is applied to week numbers. When weekday format is not provided then
   is used default format: '{:%W}' when week starts with Monday, '{:%U}' otherwise
|[ *weekdays*
:[ string
:[
:[ Format is applied to weeks header(Su,Mo,...etc.)
|[ *today*
:[ string
:[ *<b><u>{}</u></b>*
:[ Format is applied to Today

## Actions

[- *String*
:- *Action*
|[ *mode*
:[ Switch calendar mode between year/month
|[ *tz_up*
:[ Switch to the next provided time zone
|[ *tz_down*
:[ Switch to the previously provided time zone
|[ *shift_up*
:[ Switch to the next calendar month/year
|[ *shift_down*
:[ Switch to the previous calendar month/year

# FORMAT REPLACEMENTS

- *{calendar}*: Current month calendar
- *{tz_list}*: List of time in the rest timezones, if more than one timezone is set in the config
- *{ordinal_date}*: The current day in (English) ordinal form, e.g. 21st

# EXAMPLES

1. General

```
"clock": {
	"interval": 60,
	"format": "{:%H:%M}",
	"max-length": 25
}
```

2. Calendar

```
"clock": {
	"format": "{:%H:%M}  ",
	"format-alt": "{:%A, %B %d, %Y (%R)}  ",
	"tooltip-format": "<tt><small>{calendar}</small></tt>",
	"calendar": {
		"mode"          : "year",
		"mode-mon-col"  : 3,
		"weeks-pos"     : "right",
		"on-scroll"     : 1,
		"on-click-right": "mode",
		"format": {
			"months":     "<span color='#ffead3'><b>{}</b></span>",
			"days":       "<span color='#ecc6d9'><b>{}</b></span>",
			"weeks":      "<span color='#99ffdd'><b>W{}</b></span>",
			"weekdays":   "<span color='#ffcc66'><b>{}</b></span>",
			"today":      "<span color='#ff6699'><b><u>{}</u></b></span>"
		}
	},
	"actions": {
		"on-click-right": "mode",
		"on-click-forward": "tz_up",
		"on-click-backward": "tz_down",
		"on-scroll-up": "shift_up",
		"on-scroll-down": "shift_down"
	}
},
```

3. Full date on hover

```
"clock": {
	"interval": 60,
	"tooltip": true,
	"format": "{:%H.%M}",
	"tooltip-format": "{:%Y-%m-%d}",
}
```

# STYLE

- *#clock*

# Troubleshooting

If clock module is disabled at startup with locale::facet::\_S\_create\_c\_locale ++
name not valid error message try one of the following:

- check if LC_TIME is set properly (glibc)
- set locale to C in the config file (musl)

The locale option must be set for {calendar} to use the correct start-of-week, regardless of system locale.

## Calendar in Chinese. Alignment

In order to have aligned Chinese calendar there are some useful recommendations:

. Use "WenQuanYi Zen Hei Mono" which is provided in most Linux distributions
. Try different font sizes and find best for you. size = 9pt should be fine
. In case when "WenQuanYi Zen Hei Mono" font is used disable monospace font pango tag

Example of working config

```
"clock": {
	"format": "{:%H:%M}  ",
	"format-alt": "{:%A, %B %d, %Y (%R)}  ",
	"tooltip-format": "\n<span size='9pt' font='WenQuanYi Zen Hei Mono'>{calendar}</span>",
	"calendar": {
		"mode"          : "year",
		"mode-mon-col"  : 3,
		"weeks-pos"     : "right",
		"on-scroll"     : 1,
		"on-click-right": "mode",
		"format": {
			"months":     "<span color='#ffead3'><b>{}</b></span>",
			"days":       "<span color='#ecc6d9'><b>{}</b></span>",
			"weeks":      "<span color='#99ffdd'><b>W{}</b></span>",
			"weekdays":   "<span color='#ffcc66'><b>{}</b></span>",
			"today":      "<span color='#ff6699'><b><u>{}</u></b></span>"
		}
	},
	"actions": {
		"on-click-right": "mode",
		"on-click-forward": "tz_up",
		"on-click-backward": "tz_down",
		"on-scroll-up": "shift_up",
		"on-scroll-down": "shift_down"
	}
},
```

# AUTHOR

Alexis Rouillard <contact@arouillard.fr>
