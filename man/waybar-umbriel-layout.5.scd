waybar-umbriel-layout(5)

# NAME

waybar - umbriel layout module

# DESCRIPTION

The *layout* module displays the layout of the active workspace on the bar's output.

Umbriel reports one of *scrolling*, *dwindle*, *master*, or *stacking* for each
workspace. The module hides itself while the output has no active workspace.

# CONFIGURATION

Addressed by *umbriel/layout*

*format*: ++
	typeof: string ++
	default: {} ++
	The default format, how the layout should be displayed. The positional *{}*
	(the default) and *{layout}* are both replaced by the current layout name.

*format-<layout>*: ++
	typeof: string ++
	default: *none* ++
	The custom format to use when a specific layout is active (for example,
	*format-scrolling*, *format-dwindle*, *format-master*, or *format-stacking*).
	Keys use Umbriel's lowercase layout names and are case-sensitive. If no match
	is found, it falls back to *format*.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables the module to consume all leftover space.

# FORMAT REPLACEMENTS

*{layout}*: The active workspace's layout: *scrolling*, *dwindle*, *master*, or
*stacking*.

# STYLE

The current layout name is added as a dynamic CSS class.

- *#layout*
- *#layout.scrolling*
- *#layout.dwindle*
- *#layout.master*
- *#layout.stacking*

# EXAMPLES

```
"umbriel/layout": {
    "format": "{layout}",
    "format-scrolling": "Scrolling",
    "format-dwindle": "Dwindle",
    "format-master": "Master",
    "format-stacking": "Stacking"
}
```

## CSS Example

```
#layout {
    color: #ffffff;
    padding: 0 5px;
}

#layout.scrolling {
    color: #a6e3a1;
}

#layout.dwindle {
    color: #89b4fa;
}

#layout.master {
    color: #f9e2af;
}

#layout.stacking {
    color: #f38ba8;
}
```
