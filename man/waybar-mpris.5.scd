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

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	default: {player} ({status}) {dynamic} ++
	The tooltip text format.

*tooltip-format-[status]*: ++
	typeof: string ++
	default: "MPD (disconnected)" ++
	The status-specif tooltip format.

*artist-len*: ++
	typeof: integer ++
	Maximum length of the Artist tag.

*album-len*: ++
	typeof: integer ++
	Maximum length of the Album tag.

*title-len*: ++
	typeof: integer ++
	Maximum length of the Title tag.

*dynamic-len*: ++
	typeof: integer ++
	Maximum length of the Dynamic tag.

*dynamic-priority* ++ 
	typeof: []string ++
	default: ["title", "length", "artist", "album"]
	Priority of the tags when truncating the Dynamic tag (absence in this
	list means force inclusion).

*enable-tooltip-len-limits*: ++
	typeof: bool ++
	default: false ++
	Option to enable the limits for the tooltip as well

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
	"format": "{player_icon} {dynamic}",
	"format-paused": "{status_icon} <i>{dynamic}</i>",
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
