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
	This format is used when there are no failing units.

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
	Option to hide this module when there are no failed units.

*tooltip-format*: ++
	typeof: string ++
	default: *System: {system_state}\nUser: {user_state}\nFailed units ({nr_failed}):\n{failed_units_list}* ++
	Tooltip format shown when there are failed units.

*tooltip-format-ok*: ++
	typeof: string ++
	default: *System: {system_state}\nUser: {user_state}* ++
	Tooltip format used when there are no failed units.

*tooltip-unit-format*: ++
	typeof: string ++
	default: *{name}: {description}* ++
	Format used to render each failed unit inside the tooltip. Each item is prefixed with a bullet.

*menu*: ++
	typeof: string ++
	Action that pops up the menu.

*menu-file*: ++
	typeof: string ++
	Location of the menu descriptor file. There needs to be an element of type
	GtkMenu with id *menu*

*menu-actions*: ++
	typeof: array ++
	The actions corresponding to the buttons of the menu.

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all leftover space dynamically.

# FORMAT REPLACEMENTS

*{nr_failed_system}*: Number of failed units from systemwide (PID=1) systemd.

*{nr_failed_user}*: Number of failed units from user systemd.

*{nr_failed}*: Number of total failed units.

*{system_state}:* State of the systemd system session.

*{user_state}:* State of the systemd user session.

*{overall_state}:* Overall state of the systemd and user session. ("ok" or "degraded")

*{failed_units_list}:* Bulleted list of failed units using *tooltip-unit-format*. Empty when
there are no failed units.

The *tooltip-unit-format* string supports the following replacements:

*{name}*: Unit name ++
*{description}*: Unit description ++
*{load_state}*: Unit load state ++
*{active_state}*: Unit active state ++
*{sub_state}*: Unit sub state ++
*{scope}*: Either *system* or *user* depending on where the unit originated

# EXAMPLES

```
"systemd-failed-units": {
	"hide-on-ok": false,
	"format": "✗ {nr_failed}",
	"format-ok": "✓",
	"system": true,
	"user": false,
	"tooltip-format": "{nr_failed} failed units:\n{failed_units_list}",
	"tooltip-unit-format": "{scope}: {name} ({active_state})",
}
```

# STYLE

- *#systemd-failed-units*
- *#systemd-failed-units.ok*
- *#systemd-failed-units.degraded*
