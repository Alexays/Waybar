waybar-states(5)

# NAME

waybar - states property

# OVERVIEW

Some modules support 'states' which allows percentage values to be used as styling triggers to
apply a class when the value matches the declared state value.

# STATES

Every entry (*state*) consists of a *<name>* (typeof: *string*) and a *<value>* (typeof: *integer*).

- The state can be addressed as a CSS class in the *style.css*. The name of the CSS class is the *<name>* of the state.
  Each class gets activated when the current value is equal to or less than the configured *<value>* for the *battery* module, or equal to or greater than the configured *<value>* for all other modules.

- Also, each state can have its own *format*.
  Those can be configured via *format-<name>*, or if you want to differentiate a bit more, as *format-<status>-<state>*.

# EXAMPLE

```
"battery": {
	"bat": "BAT2",
	"interval": 60,
	"states": {
		"warning": 30,
		"critical": 15
	},
	"format": "{capacity}% {icon}",
	"format-icons": ["", "", "", "", ""],
	"max-length": 25
}
```

# STYLING STATES

- *#battery.<state>*
  - *<state>* can be defined in the *config*.

# EXAMPLE:

- *#battery.warning: { background: orange; }*
- *#battery.critical: { background: red; }*
