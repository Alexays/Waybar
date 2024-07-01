waybar-upower(5)

# NAME

waybar - upower module

# DESCRIPTION

The *upower* module displays the main battery capacity with all other upower
compatible devices in the tooltip.

# CONFIGURATION

*native-path*: ++
	typeof: string ++
	default: ++
	The battery to monitor. Refer to the https://upower.freedesktop.org/docs/UpDevice.html#UpDevice--native-path ++
	Can be obtained using `upower --dump`

*model*: ++
	typeof: string ++
	default: ++
	The battery to monitor, based on the model. (this option is ignored if *native-path* is given). ++
	Can be obtained using `upower --dump`

*icon-size*: ++
	typeof: integer ++
	default: 20 ++
	Defines the size of the icons.

*format*: ++
	typeof: string ++
	default: {percentage} ++
	The text format.

*format-alt*: ++
	typeof: string ++
	default: {percentage} {time} ++
	The text format when toggled.

*hide-if-empty*: ++
	typeof: bool ++
	default: true ++
	Defines visibility of the module if no devices can be found.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-spacing*: ++
	typeof: integer ++
	default: 4 ++
	Defines the spacing between the tooltip device name and device battery ++
	status.

*tooltip-padding*: ++
	typeof: integer ++
	default: 4 ++
	Defines the spacing between the tooltip window edge and the tooltip content.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*show-icon*: ++
	typeof: bool ++
	default: true ++
	Option to disable battery icon.

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

*{percentage}*: The battery capacity in percentage

*{time}*: An estimated time either until empty or until fully charged ++
depending on the charging state.

# EXAMPLES

```
"upower": {
	"icon-size": 20,
	"hide-if-empty": true,
	"tooltip": true,
	"tooltip-spacing": 20
}

```
```
"upower": {
	"native-path": "/org/bluez/hci0/dev_D4_AE_41_38_D0_EF",
	"icon-size": 20,
	"hide-if-empty": true,
	"tooltip": true,
	"tooltip-spacing": 20
}

```
```
"upower": {
	"native-path": "battery_sony_controller_battery_d0o27o88o32ofcoee",
	"icon-size": 20,
	"hide-if-empty": true,
	"tooltip": true,
	"tooltip-spacing": 20
}
```
```
"upower": {
	"show-icon": false,
	"hide-if-empty": true,
	"tooltip": true,
	"tooltip-spacing": 20
}

```

# STYLE

- *#upower*
- *#upower.charging*
- *#upower.discharging*
- *#upower.full*
- *#upower.empty*
- *#upower.unknown-status*
