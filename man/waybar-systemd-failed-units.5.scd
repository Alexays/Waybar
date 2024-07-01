waybar-systemd-failed-units(5)

# NAME

waybar - systemd failed units monitor module

# DESCRIPTION

The *systemd-failed-units* module displays the number of failed systemd units.

# CONFIGURATION

Addressed by *systemd-failed-units*

*format*: ++
	typeof: string  ++
	default: *{nr_failed} failed* ++
	The format, how information should be displayed. This format is used when other formats aren't specified.

*format-ok*: ++
	typeof: string ++
	This format is used when there is no failing units.

*user*: ++
	typeof: bool ++
	default: *true* ++
	Option to count user systemd units.

*system*: ++
	typeof: bool ++
	default: *true* ++
	Option to count systemwide (PID=1) systemd units.

*hide-on-ok*: ++
	typeof: bool ++
	default: *true* ++
	Option to hide this module when there is no failing units.

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

*{nr_failed_system}*: Number of failed units from systemwide (PID=1) systemd.

*{nr_failed_user}*: Number of failed units from user systemd.

*{nr_failed}*: Number of total failed units.

# EXAMPLES

```
"systemd-failed-units": {
	"hide-on-ok": false,
	"format": "✗ {nr_failed}",
	"format-ok": "✓",
	"system": true,
	"user": false,
}
```

# STYLE

- *#systemd-failed-units*
- *#systemd-failed-units.ok*
- *#systemd-failed-units.degraded*
