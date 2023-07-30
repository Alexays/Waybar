waybar-image(5)

# NAME

waybar - image module

# DESCRIPTION

The *image* module displays an image from a path.

# CONFIGURATION

*path*: ++
	typeof: string ++
	The path to the image.

*exec*: ++
	typeof: string ++
	The path to the script, which should return image path file. ++
	It will only execute if the path is not set

*size*: ++
	typeof: integer ++
	The width/height to render the image.

*interval*: ++
	typeof: integer ++
	The interval (in seconds) to re-render the image. ++
	This is useful if the contents of *path* changes. ++
	If no *interval* is defined, the image will only be rendered once.

*signal*: ++
	typeof: integer ++
	The signal number used to update the module. ++
	This can be used instead of *interval* if the file changes irregularly. ++
	The number is valid between 1 and N, where *SIGRTMIN+N* = *SIGRTMAX*.

*on-click*: ++
	typeof: string ++
	Command to execute when clicked on the module.

*on-click-middle*: ++
	typeof: string ++
	Command to execute when middle-clicked on the module using mousewheel.

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
	Option to enable tooltip on hover.

# SCRIPT OUTPUT

Similar to the *custom* module, output values of the script is *newline* separated.
The following is the output format:

```
$path\\n$tooltip
```

# EXAMPLES

```
"image#album-art": {
	"path": "/tmp/mpd_art",
	"size": 32,
	"interval": 5,
	"on-click": "mpc toggle"
}
```

# STYLE

- *#image*
