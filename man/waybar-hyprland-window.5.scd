waybar-hyprland-window(5)

# NAME

waybar - hyprland window module

# DESCRIPTION

The *window* module displays the title of the currently focused window in Hyprland.

# CONFIGURATION

Addressed by *hyprland/window*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} the current window title is displayed.

*rewrite*: ++
	typeof: object ++
	Rules to rewrite window title. See *rewrite rules*.

# REWRITE RULES

*rewrite* is an object where keys are regular expressions and values are
rewrite rules if the expression matches. Rules may contain references to
captures of the expression.

Regular expression and replacement follow ECMA-script rules.

If no expression matches, the title is left unchanged.

Invalid expressions (e.g., mismatched parentheses) are skipped.

# EXAMPLES

```
"hyprland/window": {
    "format": "{}",
    "rewrite": {
      "(.*) - Mozilla Firefox": "🌎 $1",
      "(.*) - zsh": "> [$1]"
    }
}
```

# STYLE

- *#window*
