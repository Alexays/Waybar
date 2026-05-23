waybar-triad-language(5)

# NAME

waybar - Triad language module

# DESCRIPTION

The *language* module shows Triad's current keyboard layout. It reads the layout
list and selected index from Triad's native IPC state.

# CONFIGURATION

Addressed by *triad/language*

*format*: ++
	typeof: string ++
	default: {} ++
	The text to display.

*format-<lang>* ++
	typeof: string++
	An alternate label for a layout. The suffix is the short language
	description, such as *en* or *de*.

*menu*: ++
	typeof: string ++
	Action that opens the menu.

*menu-file*: ++
	typeof: string ++
	Path to the menu descriptor. It must contain a GtkMenu element with the ID
	*menu*.

*menu-actions*: ++
	typeof: array ++
	Actions for the menu buttons.

*expand*: ++
	typeof: bool ++
	default: false ++
	Allows this module to take remaining bar space.

# FORMAT REPLACEMENTS

*{short}*: Short layout name, such as "us". This is also used for {}.

*{shortDescription}*: Short layout description, such as "en".

*{long}*: Long layout name, such as "English (Dvorak)".

*{variant}*: Layout variant, such as "dvorak".

# EXAMPLES

```
"triad/language": {
	"format": "{short}",
	"format-en": "EN",
	"format-de": "DE"
}
```

# STYLE

- *#language*

The widget also gets a CSS class matching the current layout's short name:

```
#language.us { color: #00ff00; }
#language.de { color: #ff0000; }
```
