waybar-hyprland-submap(5)

# NAME

waybar - hyprland submap module

# DESCRIPTION

The *submap* module displays the currently active submap similar to *sway/mode*.

# CONFIGURATION

Addressed by *hyprland/submap*

*format*: ++
	typeof: string ++
	default: {} ++
	The format, how information should be displayed. On {} the currently active submap is displayed.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label.

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
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

*on-click-right*: ++
	typeof: string ++
	Command to execute when you right-click on the module.

*on-update*: ++
	typeof: string ++
	Command to execute when the module is updated.

*on-scroll-up*: ++
	typeof: string ++
	Command to execute when scrolling up on the module.

*on-scroll-down*: ++
	typeof: string ++
	Command to execute when scrolling down on the module.

*smooth-scrolling-threshold*: ++
	typeof: double ++
	Threshold to be used when scrolling.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*always-on*: ++
	typeof: bool ++
	default: false ++
	Option to display the widget even when there's no active submap.

*default-submap* ++
	typeof: string ++
	default: Default ++
	Option to set the submap name to display when not in an active submap.

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


# EXAMPLES

```
"hyprland/submap": {
	"format": "✌️ {}",
	"max-length": 8,
	"tooltip": false
}
```

# STYLE

- *#submap*
- *#submap.<name>*
