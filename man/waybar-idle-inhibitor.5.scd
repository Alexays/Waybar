waybar-idle-inhibitor(5)

# NAME

waybar - idle_inhibitor module

# DESCRIPTION

The *idle_inhibitor* module can inhibit the idle behavior such as screen blanking, locking, and
screensaver, also known as "presentation mode".

# CONFIGURATION

*format*: ++
	typeof: string ++
	The format, how the state should be displayed.

*format-icons*: ++
	typeof: array ++
	Based on the current state, the corresponding icon gets selected.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label (in 90 degree increments).

*max-length*: ++
	typeof: integer ++
	The maximum length in character the module should display.

*min-length*: ++
	typeof: integer ++
	The minimum length in characters the module should accept.

*align*: ++
	typeof: float ++
	The alignment of the label within the module, where 0 is left-aligned and 1 is right-aligned. If the module is rotated, it will follow the flow of the text.

*justify*: ++
	typeof: string ++
	The alignment of the text within the module's label, allowing options 'left', 'right', or 'center' to define the positioning.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module. A click also toggles the state (enable only if dynamic timeouts are enabled).

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel. (reset the timeout to the initial value if dynamic timeouts are enabled).

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right-click on the module. (deactivate the inhibit if dynamic timeouts are enabled).

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when scrolling up on the module. (increase the timeout if dynamic timeouts are enabled).

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module. (decrease the timeout if dynamic timeouts are enabled).

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

*start-activated*: ++
	typeof: bool ++
	default: *false* ++
	Whether the inhibit should be activated when starting waybar.

*timeout*: ++
	typeof: double ++
	The number of minutes the inhibition should last.

*timeout_step*: ++
	typeof: double ++
	default: 10 ++
	The number of minutes to add or subtract when scrolling (when dynamic timeouts are enabled).

*dynamic-timeouts*: ++
	typeof: bool ++
	default: false ++
	Option to enable dynamic timeouts.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format-activated*: ++
	typeof: string ++
	This format is used when the inhibit is activated.

*tooltip-format-deactivated*: ++
	typeof: string ++
	This format is used when the inhibit is deactivated.

*menu*: ++
	typeof: string ++
	Action that popups the menu. Cannot be "on-click".

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file. There need to be an element of type
	GtkMenu with id *menu*

*menu-actions*: ++
	typeof: array ++
	The actions corresponding to the buttons of the menu.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# FORMAT REPLACEMENTS

*{status}*: status (*activated* or *deactivated*)

*{icon}*: Icon, as defined in *format-icons*

*{timeout}*: Timeout in minutes

*{timeleft}*: Time left in minutes

# EXAMPLES

```
"idle_inhibitor": {
	"format": "{icon}",
	"format-icons": {
		"activated": "",
		"deactivated": ""
	},
	"timeout": 30.5
}
```

```
"idle_inhibitor": {
	"format": "{status} {timeleft}/{timeout}",
	"format-icons": {
		"activated": "",
		"deactivated": ""
	},
	"timeout": 480,
	"dynamic-timeouts": true,
	"timeout-step": 10
}
```
