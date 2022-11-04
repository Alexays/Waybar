waybar-gamemode(5)

# NAME

waybar - gamemode module

# DESCRIPTION

The *gamemode* module displays if any game or application is running with ++
Feral Gamemode optimizations.

# CONFIGURATION

*format*: ++
	typeof: string ++
	default: {glyph} ++
	The text format.

*format-alt*: ++
	typeof: string ++
	default: {glyph} {count} ++
	The text format when toggled.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string ++
	default: Games running: {glyph} ++
	The text format of the tooltip.

*hide-not-running*: ++
	typeof: bool ++
	default: true ++
	Defines if the module should be hidden if no games are running.

*use-icon*: ++
	typeof: bool ++
	default: true ++
	Defines if the module should display a GTK icon instead of the specified *glyph*

*glyph*: ++
	typeof: string ++
	default:  ++
	The string icon to display. Only visible if *use-icon* is set to false.

*icon-name*: ++
	typeof: string ++
	default: input-gaming-symbolic ++
	The GTK icon to display. Only visible if *use-icon* is set to true.

*icon-size*: ++
	typeof: unsigned integer ++
	default: 20 ++
	Defines the size of the icons.

*icon-spacing*: ++
	typeof: unsigned integer ++
	default: 4 ++
	Defines the spacing between the icon and the text.

# FORMAT REPLACEMENTS

*{glyph}*: The string icon glyph to use instead.

*{count}*: The amount of games running with gamemode optimizations.

# TOOLTIP FORMAT REPLACEMENTS

*{count}*: The amount of games running with gamemode optimizations.

# EXAMPLES

```
"gamemode": {
	"format": "{glyph}",
	"format-alt": "{glyph} {count}",
	"glyph": "",
	"hide-not-running": true,
	"use-icon": true,
	"icon-name": "input-gaming-symbolic",
	"icon-spacing": 4,
	"icon-size": 20,
	"tooltip": true,
	"tooltip-format": "Games running: {count}"
}

```

# STYLE

- *#gamemode*
- *#gamemode.running*
