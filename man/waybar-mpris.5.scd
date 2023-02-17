waybar-mpris(5)

# NAME

waybar - MPRIS module

# DESCRIPTION

The *mpris* module displays currently playing media via libplayerctl.

# CONFIGURATION

*player*: ++
	typeof: string ++
	default: playerctld ++
	Name of the MPRIS player to attach to. Using the default value always
	follows the currenly active player.

*ignored-players*: ++
	typeof: []string ++
	Ignore updates of the listed players, when using playerctld.

*interval*: ++
	typeof: integer ++
	Refresh MPRIS information on a timer.

*format*: ++
	typeof: string ++
	default: {player} ({status}) {dynamic} ++
	The text format.

*format-[status]*: ++
	typeof: string ++
	The status-specific text format.

*on-click*: ++
	typeof: string ++
	default: play-pause ++
	Overwrite default action toggles.

*on-middle-click*: ++
	typeof: string ++
	default: previous track ++
	Overwrite default action toggles.

*on-right-click*: ++
	typeof: string ++
	default: next track ++
	Overwrite default action toggles.

*player-icons*: ++
	typeof: map[string]string
	Allows setting _{player-icon}_ based on player-name property.

*status-icons*: ++
	typeof: map[string]string
	Allows setting _{status-icon}_ based on player status (playing, paused,
	stopped).


# FORMAT REPLACEMENTS

*{player}*: The name of the current media player

*{status}*: The current status (playing, paused, stopped)

*{artist}*: The artist of the current track

*{album}*: The album title of the current track

*{title}*: The title of the current track

*{length}*: Length of the track, formatted as HH:MM:SS

*{dynamic}*: Use _{artist}_, _{album}_, _{title}_ and _{length}_, automatically omit++
           empty values

*{player-icon}*: Chooses an icon from _player-icons_ based on _{player}_

*{status-icon}*: Chooses an icon from _status-icons_ based on _{status}_

# EXAMPLES

```
"mpris": {
	"format": "DEFAULT: {player_icon} {dynamic}",
	"format-paused": "DEFAULT: {status_icon} <i>{dynamic}</i>",
	"player-icons": {
		"default": "▶",
		"mpv": "🎵"
	},
	"status-icons": {
		"paused": "⏸"
	},
	// "ignored-players": ["firefox"]
}
```

# STYLE

- *#mpris*
- *#mpris.${status}*
- *#mpris.${player}*
