waybar-image(5)

# NAME

waybar - image module

# DESCRIPTION

The *image* module displays container of images(or image) from a provided paths

# REMARK

This module has been rewritten to add multiple image rendering functionality.
To avoid users inconvenience of breaking changes, everything related to old(only
one *image*) implementation has been left untouched including configuration part. For this
reason this wiki page is split into two parts

# CONFIGURATION

For single *image*

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

Similar to the *custom* module, output values of the script are *newline* separated.
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
- *#image.empty*

# CONFIGURATION

For multiple *image*

*size*: ++
	typeof: string ++
	default: 16 ++
	Minumum size of the rendered image in pixels.

*interval*: ++
	typeof: interger ++
	default: INT_MAX ++
	The interval in seconds to redraw module ++
	Default value is max value of int and changing it is only recommended ++
	if image path or other property is being changed overtime ++
	If no *interval* is provided, the module will only be rendered once.

*multiple*: ++
	typeof: bool ++
	default: false ++
	this parameter is used to decide if old *image* implementation should be ++
	used(therefore render only one image) or to pick a new implementation to ++
	rended multiple images provided from this config. default value of this ++
	parameter is false.

*signal*: ++
	typeof: interger ++
	default: 0 ++
	The signal number is used to redraw the module. ++
	This is used if *interval* is not provided and module should change ++
	regularly depended on external events, e.g when clicking on it. ++
	The provided value is valid between 1 and N, where SIGRTMIN+N <= SIGRTMAX. ++
	e.g if *signal* = 7, 34 + 7 = 41, which means waybar process should	++
	receive signal 41 to redraw this module, since SIGRTMIN = 34 (see 'kill -l').

*entries*: ++
	typeof: array ++
	default: empty array ++
	Json array of objects consisting of several fields ++
	this is a main part of configuration where per image properties are defined

	*path*: ++
	typeof: string ++
	default: empty string ++
	file path of picture to draw

	*marker*: ++
	typeof: string ++
	default: empty string ++
	user defined per image keyword(marker) to identify them, e.g in styles.css ++
	e.g if you want to draw 3 pictures and one of them dimmed, you can give ++
	*marker*: "dimmed" value and in styles.css write something like this ++
	#image .dimmed { opacity: 0.3 }

	*tooltip*: ++
	typeof: string ++
	default: empty string ++
	description of the image displayed on hover

	*on-click*: ++
	typeof: string ++
	default: empty string ++
	action to perform when clicking on the image. ++
	The action can be any system or custom binary/script ++
	e.g if { "on-click": "notify-send \"hello world\"" } ++
	"hello world" will appear as notification.

*exec*: ++
	typeof: string ++
	default: empty string ++
	same as *entries* but provided from external script which should dynamically ++
	return json array of objects consisting of same keys as *entries*. ++
	This option is specifically useful in combination with *signal* option without *interval* provided.

# Examples

With entries
```
"image#wentr": {
  "entries": [
    {
      "path": "/home/user/Pictures/idk1.png",
      "marker": "dimmed",
      "tooltip": "dimmed image",
      "on-click": "notify-send \"hello world\""
    },
    {
      "path": "/home/user/Pictures/idk2.png",
      "marker": "normal",
      "on-click": "myCustomScript.py"
    }
  ],
  "size": 32,
  "interval": 1 // will redraw every 5 seconds
}
```

With exec
```
"image#wexec": {
  "exec": "imageProvider.py",
  "size": 32,
  "signal": 7 // e.g 'pkill -n waybar --signal 41' will trigger redraw
}
```

where imageProvider.py looks like this
```
#!/usr/bin/python

import json

dir = "/home/user/Pictures/"
out = [
    {"path": dir + "idk1.png", "marker": "clickable", "on-click": "echo 'hello world'"},
    {"path": dir + "idk2.png", "marker": "normal", "tooltip": "sample tooltip"},
    {"path": dir + "idk3.png", "marker": "important"},
]
res = json.dumps(out)
print(res)
```

## Example styles
```
#image .important {
    opacity: 0.9;
}

#image button.important {
    opacity: 0.2
}

#image .normal {
    opacity: 0.3
}
```

# STYLE

- *#image*
- *#image*.empty /\*when image could not be rendered\*/
- *#image*.button /\*when on-click is provided\*/
- *#image*.*marker* /\*where *marker* is parameter from config\*/
- *#image*.button.*marker*

