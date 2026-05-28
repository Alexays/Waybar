waybar-mango-language(5)

# NAME

waybar - mango language module

# DESCRIPTION

The *language* module displays the currently active keyboard layout in the Mango compositor.

# CONFIGURATION

Addressed by *mango/language*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how the layout should be displayed. See *FORMAT REPLACEMENTS*.

*format-<lang>*: ++
	typeof: string ++
	Provide an alternative format string for a given language.
	<lang> is the short description of the layout (e.g. "us", "de").
	The value is used as the replacement for *{}* in the main *format*.
	This option can be repeated for multiple languages.

*menu*: ++
	typeof: string ++
	Action that pops up a menu.

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file. It must contain an element of type GtkMenu with id *menu*.

*menu-actions*: ++
	typeof: array ++
	Actions corresponding to the buttons of the menu.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all leftover space dynamically.

# FORMAT REPLACEMENTS

*{short}*: Short name of the layout (e.g. "us"). This is also the default when no format is specified.

*{shortDescription}*: Short description of the layout (same as *{short}* in most cases).

*{long}*: Full name of the layout as reported by Mango (e.g. "English (US)").

*{variant}*: Variant of the layout, if any.

# EXAMPLES

```
"mango/language": {
	"format": " {long} ",
	"format-us": "US",
	"format-de": "DE"
}
```

# STYLE

- *#language*

A CSS class matching the current layout's short name is added to the widget.
This allows per‑layout styling:

```
#language.us { color: #00ff00; }
#language.de { color: #ff0000; }
```
