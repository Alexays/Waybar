waybar-libvirt(5)

# NAME

waybar - libvirt module

# DESCRIPTION

The *libvirt* conncets to a given libvirt uri and retrives information about
virtual machines.

# CONFIGURATION

Addressed by *libvirt*

*virt_uri*: ++
	typeof: string ++
	The uri for libvirt to connect to, such as *qemu:///system*.

*domains*: ++
	tyepof: string[]
	List of domain names to be included for inspection. If empty, use all.

*unit_size*: ++
	typeof: string ++
	default: "G" ++
	The scaling factor for units of size. Valid scalings are: k | kB, kiB, M|MB, G|GB, GiB, T|TB, TiB.

*unit_time*: ++
	typeof: string ++
	default: "s" ++
	The scaling factor for units of time. Valid scalings are: us, ns, s, m, h, d.

*precision_size*: ++
	tyepof: unsigned int
	default: 2 ++
	The precision of size information displayed after the comma, e.g. "1.11G".

* precision_time* ++
	typeof: unsigned int
	default: 2 ++
	The precision of time information displayed after the comma, e.g. "1.11s".

*interval*: ++
	typeof: integer ++
	default: 10 ++
	The interval in which the information gets polled.

*format*: ++
	typeof: string  ++
	The format in which the VM domain information should be displayed.

*tooltip-format*: ++
	typeof: string  ++
	default: "{mem_cur} used out of {mem_max}({domains})\nAll Domains:\n" plus list of all VM domains ++
	The default tooltip show all VM domains that were found in there full representation as a list.

*rotate*: ++
	typeof: integer ++
	Positive value to rotate the text label (in 90 degree increments).

*max-length*: ++
	typeof: integer ++
	The maximum length in characters the module should display.

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
	Command to execute when you click on the module.

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

*expand*: ++
	typeof: bool ++
	default: false ++
	Enables this module to consume all left over space dynamically.

# FORMAT REPLACEMENTS

*{mam_max}*: Maximum allocatable memory of all VM domains in state RUNNING.

*{mem_cur}*: Current memory consumption of all VM domains in state RUNNING.

*{cpu_time}*: Accumulated CPU run time of all VM domains in state RUNNING.

Virtual machine domains in libvirt have on of the following well defined states (excluding NOSTATE):

```
    VIR_DOMAIN_NOSTATE = 0,     /* no state (Since: 0.0.1) */
    VIR_DOMAIN_RUNNING = 1,     /* the domain is running (Since: 0.0.1) */
    VIR_DOMAIN_BLOCKED = 2,     /* the domain is blocked on resource (Since: 0.0.1) */
    VIR_DOMAIN_PAUSED  = 3,     /* the domain is paused by user (Since: 0.0.1) */
    VIR_DOMAIN_SHUTDOWN= 4,     /* the domain is being shut down (Since: 0.0.1) */
    VIR_DOMAIN_SHUTOFF = 5,     /* the domain is shut off (Since: 0.0.1) */
    VIR_DOMAIN_CRASHED = 6,     /* the domain is crashed (Since: 0.0.2) */
    VIR_DOMAIN_PMSUSPENDED = 7, /* the domain is suspended by guest power management (Since: 0.9.11) */
```

*{domains_running}*: Number of all VM domains with state RUNNING.

*{domains_blocked}*: Number of all VM domains with state BLOCKED.

*{domains_paused}*: Number of all VM domains with state PAUSED.

*{domains_shutdown}*: Number of all VM domains with state SHUTDOWN.

*{domains_crashed}*: Number of all VM domains with state CRASHED.

*{domains_suspended}*: Number of all VM domains with state PMSUSPENDED.

*{domains}*: Number of all VM domains in any state defined on the *virt_uri*.

# EXAMPLES

```
"libvirt": {
	"virt_uri": "qemu:///system",
	"domains": [],
	"format": "{mem_cur:2.f}/{mem_max:2.f} ({domains})",
	"unit_time": "s",
	"unit_size": "G",
	"precision_time": 2,
	"precision_size": 2,
	"interval": 30
}
```

# STYLE

- *#libvirt*
