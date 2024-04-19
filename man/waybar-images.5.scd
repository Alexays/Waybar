waybar-images(5)

# NAME

waybar - images module

# DESCRIPTION

The *images* module displays container of images(or image) from a provided paths

# REMARK

This module is inspired from another waybar module called *image*

# Configuration

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
	#images .dimmed { opacity: 0.3 }

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
"images#wentr": {
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
"images#wexec": {
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
#images .important {
    opacity: 0.9;
}

#images button.important {
    opacity: 0.2
}

#images .normal {
    opacity: 0.3
}
```

# STYLE

- *#images*
- *#images*.empty /\*when image could not rendered\*/
- *#images*.button /\*when on-click is provided\*/
- *#images*.*marker* /\*where *marker* is parameter from config\*/
- *#images*.button.*marker*

