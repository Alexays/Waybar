waybar-sway-language(5)

# NAME

waybar - sway language module

# DESCRIPTION

The *language* module displays the current keyboard layout in Sway

# CONFIGURATION

Addressed by *sway/language*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how layout should be displayed.

*hide-single-layout*: ++
	typeof: bool ++
	default: false ++
	Defines visibility of the module if a single layout is configured

*tooltip-format*: ++
	typeof: string ++
	default: {} ++
	The format, how layout should be displayed in tooltip.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

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

*{short}*: Short name of layout (e.g. "us"). Equals to {}.

*{shortDescription}*: Short description of layout (e.g. "en").

*{long}*: Long name of layout (e.g. "English (Dvorak)").

*{variant}*: Variant of layout (e.g. "dvorak").

*{flag}*: Country flag of layout.

# EXAMPLES

```
"sway/language": {
	"format": "{}",
},

"sway/language": {
	"format": "{short} {variant}",
}
```

# STYLE

- *#language*
