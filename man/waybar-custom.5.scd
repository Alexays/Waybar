waybar-custom(5)
# NAME

waybar - custom module

# DESCRIPTION

The *custom* module displays either the output of a script or static text.
To display static text, specify only the *format* field.

# CONFIGURATION

Addressed by *custom/<name>*

*exec*: ++
	typeof: string ++
	The path to the script, which should be executed.

*exec-if*: ++
	typeof: string ++
	The path to a script, which determines if the script in *exec* should be executed. ++
	*exec* will be executed if the exit code of *exec-if* equals 0.

*hide-empty-text*: ++
	typeof: bool ++
	Disables the module when output is empty, but format might contain additional static content.

*exec-on-event*: ++
	typeof: bool ++
	default: true ++
	If an event command is set (e.g. *on-click* or *on-scroll-up*) then re-execute the script after executing the event command.

*return-type*: ++
	typeof: string ++
	See *return-type*

*interval*: ++
	typeof: integer ++
	The interval (in seconds) in which the information gets polled. ++
	Use *once* if you want to execute the module only on startup. ++
	You can update it manually with a signal. If no *interval* or *signal* is defined, it is assumed that the out script loops itself. ++
	If a *signal* is defined then the script will run once on startup and will only update with a signal.

*restart-interval*: ++
	typeof: integer ++
	The restart interval (in seconds). ++
	Can't be used with the *interval* option, so only with continuous scripts. ++
	Once the script exits, it'll be re-executed after the *restart-interval*.

*signal*: ++
	typeof: integer ++
	The signal number used to update the module. ++
	The number is valid between 1 and N, where *SIGRTMIN+N* = *SIGRTMAX*. ++
	If no interval is defined then a signal will be the only way to update the module.

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} data gets inserted.

*format-icons*: ++
	typeof: array ++
	Based on the set percentage, the corresponding icon gets selected. The order is *low* to *high*.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*min-length*: ++
	typeof: integer ++
	The minimum length in characters the module should accept.

*align*: ++
	typeof: float ++
	The alignment of the label within the module, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*justify*: ++
	typeof: string ++
	The alignment of the text within the module's label, allowing options 'left', 'right', or 'center' to define the positioning.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right-click on the module.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when scrolling up on the module.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	The tooltip format. If specified, overrides any tooltip output from the script in *exec*. ++
	Uses the same format replacements as *format*.

*escape*: ++
	typeof: bool ++
	default: false ++
	Option to enable escaping of script output.

*menu*: ++
	typeof: string ++
	Action that popups the menu.

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file. There need to be an element of type
	GtkMenu with id *menu*

*menu-actions*: ++
	typeof: array ++
	The actions corresponding to the buttons of the menu.

# RETURN-TYPE

When *return-type* is set to *json*, Waybar expects the *exec*-script  to output its data in JSON format.
This should look like this:

```
{"text": "$text", "tooltip": "$tooltip", "class": "$class", "percentage": $percentage }
```

The *class* parameter also accepts an array of strings.

If nothing or an invalid option is specified, Waybar expects i3blocks style output. Values are *newline* separated.
This should look like this:

```
$text\\n$tooltip\\n$class*
```

*class* is a CSS class, to apply different styles in *style.css*

# FORMAT REPLACEMENTS

*{}*: Output of the script.

*{percentage}* Percentage which can be set via a json return type.

*{icon}*: An icon from 'format-icons' according to percentage.

# EXAMPLES

## Spotify:

```
"custom/spotify": {
	"format": " {}",
	"max-length": 40,
	"interval": 30, // Remove this if your script is endless and write in loop
	"exec": "$HOME/.config/waybar/mediaplayer.sh 2> /dev/null", // Script in resources folder
	"exec-if": "pgrep spotify",
	"return-type": "json"
}
```

## mpd:

```
"custom/mpd": {
	"format": "♪ {}",
	//"max-length": 15,
	"interval": 10,
	"exec": "mpc current",
	"exec-if": "pgrep mpd",
	"on-click": "mpc toggle",
	"on-click-right": "sonata"
}
```

## cmus:

```
"custom/cmus": {
	"format": "♪ {}",
	//"max-length": 15,
	"interval": 10,
	"exec": "cmus-remote -C \"format_print '%a - %t'\"", // artist - title
	"exec-if": "pgrep cmus",
	"on-click": "cmus-remote -u",                        //toggle pause
	"escape": true                                       //handle markup entities
}
```

## Pacman

```

"custom/pacman": {
	"format": "{}  ",
	"interval": "once",
	"exec": "pacman_packages",
	"on-click": "update-system",
	"signal": 8
}
```

## Alternate Pacman

```
"custom/pacman": {
	"format": "{}  ",
	"interval": 3600,                     // every hour
	"exec": "checkupdates | wc -l",       // # of updates
	"exec-if": "exit 0",                  // always run; consider advanced run conditions
	"on-click": "termite -e 'sudo pacman -Syu'; pkill -SIGRTMIN+8 waybar", // update system
	"signal": 8
}
```

You can use the signal and update the number of available packages with *pkill -RTMIN+8 waybar*.

# STYLE

- *#custom-<name>*
- *#custom-<name>.<class>*
- *<class>* can be set by the script. For more information see *return-type*
