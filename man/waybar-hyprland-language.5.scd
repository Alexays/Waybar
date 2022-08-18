waybar-hyprland-language(5)

# NAME

waybar - hyprland language module

# DESCRIPTION

The *language* module displays the currently selected language.

# CONFIGURATION

Addressed by *hyprland/language*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} the currently selected language is displayed.

*keyboard-name*: ++
    typeof: string ++
    Specifies which keyboard to use from hyprctl devices output. Using the option that begins with "AT Translated set..." is recommended.


# EXAMPLES

```
"hyprland/language": {
    "format": "Lang: {}"
    "keyboard-name": "AT Translated Set 2 keyboard"
}
```

# STYLE

- *#language*
