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

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

# FORMAT REPLACEMENTS

*{short}*: Short name of layout (e.g. "en"). Equals to {}.

*{long}*: Long name of layout (e.g. "English (Dvorak)").

*{variant}*: Variant of layout (e.g. "Dvorak").

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
