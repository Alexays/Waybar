waybar-mango-layout(5)

# NAME

waybar - mango layout module

# DESCRIPTION

The *layout* module displays the current layout symbol of the monitor (e.g. "S", "M") in the Mango compositor.
It supports dynamic CSS classes and custom formats based on the active layout symbol.

# CONFIGURATION

Addressed by *mango/layout*

*format*: ++
	typeof: string ++
	default: {} ++
	The default format, how the layout symbol should be displayed. The positional *{}* (the default) and *{symbol}* are both replaced by the current layout symbol.

*format-<symbol>*: ++
	typeof: string ++
	default: *none* ++
	The custom format to use when a specific layout symbol is active (e.g., *format-S*, *format-M*). Note that the symbol string is strictly case-sensitive. If no match is found, it falls back to *format*.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables the module to consume all leftover space.

# FORMAT REPLACEMENTS

*{symbol}*: The layout symbol reported by Mango (e.g., "S", "M", "Dwindle").

# CUSTOM FORMATS

You can define specific formats for different layouts by appending the exact layout symbol to the *format-* key in your configuration.

For example, if your Mango compositor reports the symbol "S" for a spiral layout and "M" for a master layout, you can use *format-S* and *format-M* to define unique icons or text for each. Keep in mind that JSON keys are case-sensitive, so if the compositor sends "S", the key must be exactly *format-S*.

# STYLE

The layout module provides dynamic CSS classes based on the current layout symbol, allowing you to style each layout differently.

* *#layout*
* *.<symbol>* - The current layout symbol is dynamically added as a CSS class name (e.g., *.S*, *.M*).

# EXAMPLES

```
"mango/layout": {
    "format": "[] {symbol}",
    "format-S": "󰌌 {symbol}",
    "format-M": "󰕰 {symbol}"
}
```

## CSS Example

```
#layout {
    color: #ffffff;
    padding: 0 5px;
}

/* Specific color for the "S" layout */
#layout.S {
    color: #a6e3a1;
}

/* Specific color for the "M" layout */
#layout.M {
    color: #f38ba8;
}
```
