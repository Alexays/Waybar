waybar-mango-window(5)

# NAME

waybar - mango window module

# DESCRIPTION

The *window* module displays the title and app ID of the currently focused window in the Mango compositor.

# CONFIGURATION

Addressed by *mango/window*

*format*: ++
	typeof: string ++
	default: {title} ++
	The format string. See *FORMAT REPLACEMENTS*.

*rewrite*: ++
	typeof: object ++
	Rules to rewrite the window title. Each key is a regular expression and its value is the replacement string. Captures can be used with *$1*, *$2*, etc.

*icon*: ++
	typeof: bool ++
	default: false ++
	Whether to show the application icon.

*icon-size*: ++
	typeof: integer ++
	default: 24 ++
	Size of the application icon in pixels.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables the module to consume all leftover space.

# FORMAT REPLACEMENTS

*{title}*: The current window title.

*{app_id}*: The app ID of the focused window.

# REWRITE RULES

If the title matches a regular expression from the *rewrite* object, it is replaced by the corresponding value. Regular expression syntax follows ECMAScript rules. Unmatched titles are left unchanged.

# EXAMPLES

```
"mango/window": {
	"format": "{title}",
	"rewrite": {
		"(.*) - Mozilla Firefox": "🌎 $1",
		"(.*) - zsh": "> [$1]"
	},
	"icon": true,
	"icon-size": 20
}
```

# STYLE

- *#window*

The following classes are set on the module's event box (the container that
wraps *#window*), not on *#window* itself. Target them directly or with a
descendant selector such as *.solo #window*:

- *.empty* – applied when no window is focused (module hidden by default)
- *.solo* – applied when only one window is present on the active workspace
- *.<app-id>* – applied when a single window with the given app ID is on the workspace