waybar-mpd(5)

# NAME

waybar - mpd module

# DESCRIPTION

The *mpd* module displays information about a running "Music Player Daemon" instance.

# CONFIGURATION

Addressed by *mpd*

*server*: ++
	typeof: string ++
	The network address or Unix socket path of the MPD server. If empty, connect to the default host.

*port*: ++
	typeof: integer ++
	The port MPD listens to. If empty, use the default port.

*password*: ++
	typeof: string ++
	The password required to connect to the MPD server. If empty, no password is sent to MPD.

*interval*: ++
	typeof: integer++
	default: 5 ++
	The interval in which the connection to the MPD server is retried

*timeout*: ++
	typeof: integer++
	default: 30 ++
	The timeout for the connection. Change this if your MPD server has a low `connection_timeout` setting

*unknown-tag*: ++
	typeof: string ++
	default: "N/A" ++
	The text to display when a tag is not present in the current song, but used in `format`

*format*: ++
	typeof: string ++
	default: "{album} - {artist} - {title}" ++
	Information displayed when a song is playing.

*format-stopped*: ++
	typeof: string ++
	default: "stopped" ++
	Information displayed when the player is stopped.

*format-paused*: ++
	typeof: string ++
	This format is used when a song is paused.

*format-disconnected*: ++
	typeof: string ++
	default: "disconnected" ++
	Information displayed when the MPD server can't be reached.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	default: "MPD (connected)" ++
	Tooltip information displayed when connected to MPD.

*tooltip-format-disconnected*: ++
	typeof: string ++
	default: "MPD (disconnected)" ++
	Tooltip information displayed when the MPD server can't be reached.

*artist-len*: ++
	typeof: integer ++
	Maximum length of the Artist tag.

*album-len*: ++
	typeof: integer ++
	Maximum length of the Album tag.

*album-artist-len*: ++
	typeof: integer ++
	Maximum length of the Album Artist tag.

*title-len*: ++
	typeof: integer ++
	Maximum length of the Title tag.

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

*state-icons*: ++
	typeof: object ++
	default: {} ++
	Icon to show depending on the play/pause state of the player (*{ "playing": "...", "paused": "..." }*)

*consume-icons*: ++
	typeof: object ++
	default: {} ++
	Icon to show depending on the "consume" option (*{ "on": "...", "off": "..." }*)

*random-icons*: ++
	typeof: object ++
	default: {} ++
	Icon to show depending on the "random" option (*{ "on": "...", "off": "..." }*)

*repeat-icons*: ++
	typeof: object ++
	default: {} ++
	Icon to show depending on the "repeat" option (*{ "on": "...", "off": "..." }*)

*single-icons*: ++
	typeof: object ++
	default: {} ++
	Icon to show depending on the "single" option (*{ "on": "...", "off": "..." }*)

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

# FORMAT REPLACEMENTS

## WHEN PLAYING/PAUSED

*{artist}*: The artist of the current song

*{albumArtist}*: The artist of the current album

*{album}*: The album of the current song

*{title}*: The title of the current song

*{date}*: The date of the current song

*{volume}*: The current volume in percent

*{elapsedTime}*: The current position of the current song. To format as a date/time (see example configuration)

*{totalTime}*: The length of the current song. To format as a date/time (see example configuration)

*{songPosition}*: The position of the current song.

*{queueLength}*: The length of the current queue.

*{stateIcon}*: The icon corresponding to the playing or paused status of the player (see *state-icons* option)

*{consumeIcon}*: The icon corresponding the "consume" option (see *consume-icons* option)

*{randomIcon}*: The icon corresponding the "random" option (see *random-icons* option)

*{repeatIcon}*: The icon corresponding the "repeat" option (see *repeat-icons* option)

*{singleIcon}*: The icon corresponding the "single" option (see *single-icons* option)


## WHEN STOPPED

*{consumeIcon}*: The icon corresponding the "consume" option (see *consume-icons* option)

*{randomIcon}*: The icon corresponding the "random" option (see *random-icons* option)

*{repeatIcon}*: The icon corresponding the "repeat" option (see *repeat-icons* option)

*{singleIcon}*: The icon corresponding the "single" option (see *single-icons* option)

## WHEN DISCONNECTED

Currently, no format replacements when disconnected.

# EXAMPLES

```
"mpd": {
	"format": "{stateIcon} {consumeIcon}{randomIcon}{repeatIcon}{singleIcon}{artist} - {album} - {title} ({elapsedTime:%M:%S}/{totalTime:%M:%S}) ",
	"format-disconnected": "Disconnected ",
	"format-stopped": "{consumeIcon}{randomIcon}{repeatIcon}{singleIcon}Stopped ",
	"interval": 2,
	"consume-icons": {
		"on": " " // Icon shows only when "consume" is on
	},
	"random-icons": {
		"off": "<span color=\"#f53c3c\"></span> ", // Icon grayed out when "random" is off
		"on": " "
	},
	"repeat-icons": {
		"on": " "
	},
	"single-icons": {
		"on": "1 "
	},
	"state-icons": {
		"paused": "",
		"playing": ""
	},
	"tooltip-format": "MPD (connected)",
	"tooltip-format-disconnected": "MPD (disconnected)"
}
```

# STYLE

- *#mpd*
- *#mpd.disconnected*
- *#mpd.stopped*
- *#mpd.playing*
- *#mpd.paused*
