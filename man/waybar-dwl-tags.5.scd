waybar-dwl-tags(5)

# NAME

waybar - dwl tags module

# DESCRIPTION

The *tags* module displays the current state of tags in dwl.

# CONFIGURATION

Addressed by *dwl/tags*

*num-tags*: ++
	typeof: uint ++
	default: 9 ++
	The number of tags that should be displayed. Max 32.

*tag-labels*: ++
	typeof: array ++
	The label to display for each tag.

*disable-click*: ++
	typeof: bool ++
	default: false ++
	If set to false, you can left click to set focused tag. Right click to toggle tag focus. If set to true this behaviour is disabled.

# EXAMPLE

```
"dwl/tags": {
	"num-tags": 5
}
```

# STYLE

- *#tags button*
- *#tags button.occupied*
- *#tags button.focused*
- *#tags button.urgent*

Note that occupied/focused/urgent status may overlap. That is, a tag may be
both occupied and focused at the same time.

# SEE ALSO

waybar(5), dwl(1)
