waybar-cava(5) "waybar-cava" "User Manual"

# NAME

waybar - cava module

# DESCRIPTION

The *cava* module integrates the *karlstav/cava* audio visualizer into Waybar.
It supports two frontends: a text-based *raw* frontend and a GPU-based *GLSL*
frontend. The active frontend is selected by the *method* option in the
*[output]* section of the cava configuration file.

# FILES

$XDG_CONFIG_HOME/waybar/config ++
	Per user configuration file

# ADDITIONAL FILES

libcava lives in:

. /usr/lib/libcava.so or /usr/lib64/libcava.so
. /usr/lib/pkgconfig/cava.pc or /usr/lib64/pkgconfig/cava.pc
. /usr/include/cava

# CONFIGURATION

[- *Option*
:[ *Type*
:[ *Default*
:[ *Description*
|[ *cava_config*
:[ string
:[
:[ Path to the cava configuration file. When provided, cava settings are read from it first.
|[ *framerate*
:[ integer
:[ 30
:[ Target frames per second. Replaces the generic *interval* option.
|[ *autosens*
:[ integer
:[ 1
:[ Automatically decrease sensitivity when the bars peak.
|[ *sensitivity*
:[ integer
:[ 100
:[ Manual sensitivity in %. If *autosens* is enabled, this is only the initial value. 200 means double height. Accepts only non-negative values.
|[ *bars*
:[ integer
:[ 12
:[ The number of bars.
|[ *lower_cutoff_freq*
:[ long integer
:[ 50
:[ Lower cutoff frequency for the visualizer bandwidth.
|[ *higher_cutoff_freq*
:[ long integer
:[ 10000
:[ Higher cutoff frequency for the visualizer bandwidth.
|[ *sleep_timer*
:[ integer
:[ 5
:[ Seconds of silence before cava enters sleep mode.
|[ *hide_on_silence*
:[ bool
:[ false
:[ Hide the widget when silence lasts longer than *sleep_timer*.
|[ *format_silent*
:[ string
:[
:[ Text shown when the module is silent and *hide_on_silence* is false. **Raw frontend only.**
|[ *format-icons*
:[ array
:[
:[ Array of characters used to render bar levels in the raw frontend. The number of items determines the dynamic range.
|[ *method* \[input\]
:[ string
:[ pulse
:[ Audio capture backend. Supported values: pipewire, pulse, alsa, fifo, sndio, shmem.
|[ *source*
:[ string
:[ auto
:[ Audio source identifier. See the cava documentation for details.
|[ *sample_rate*
:[ long integer
:[ 44100
:[ See the cava documentation.
|[ *sample_bits*
:[ integer
:[ 16
:[ See the cava documentation.
|[ *stereo*
:[ bool
:[ true
:[ Enable stereo visualization.
|[ *reverse*
:[ bool
:[ false
:[ Reverse the bar order (highest frequencies on the left).
|[ *bar_delimiter*
:[ integer
:[ 0
:[ Delimiter placed between bars in the raw output. Use a decimal ASCII value (e.g. 59 = ";"). 0 means no delimiter.
|[ *monstercat*
:[ bool
:[ false
:[ Enable Monstercat smoothing.
|[ *waves*
:[ bool
:[ false
:[ Enable the waves effect alongside Monstercat smoothing.
|[ *noise_reduction*
:[ double
:[ 0.77
:[ Smoothing factor between 0.0 and 1.0. Higher values produce slower, smoother animation; lower values are more reactive but noisy.
|[ *gravity*
:[ integer
:[
:[ Gravity factor. Higher values make bars fall faster. When *noise_reduction* is set, this is derived automatically.
|[ *integral*
:[ integer
:[
:[ Integral smoothing factor. Higher values produce smoother but less precise animation. When *noise_reduction* is set, this is derived automatically.
|[ *input_delay*
:[ integer
:[ 4
:[ Delay in seconds before starting audio capture. Increase this if Waybar starts before the audio server (e.g. PipeWire).
|[ *bar_width*
:[ integer
:[
:[ Bar width in pixels. **Used by the GLSL frontend.**
|[ *bar_spacing*
:[ integer
:[
:[ Space between bars in pixels. **Used by the GLSL frontend.**
|[ *bar_height*
:[ integer
:[
:[ Ignored by Waybar. Used only by cava's "noritake" output format.
|[ *menu*
:[ string
:[
:[ Action that opens the menu.
|[ *menu-file*
:[ string
:[
:[ Location of the menu descriptor file. There must be a GtkMenu element with id *menu*.
|[ *menu-actions*
:[ array
:[
:[ Actions corresponding to the buttons of the menu.
|[ *method* \[output\]
:[ string
:[ raw
:[ Cava output method. Set to *raw* for the text frontend or *sdl_glsl* for the GPU frontend. **This is set inside the *[output]* section of the cava configuration file, not in Waybar's JSON.**
|[ *sdl_width*
:[ integer
:[
:[ GLSL frontend width in pixels. **GLSL only.**
|[ *sdl_height*
:[ integer
:[
:[ GLSL frontend height in pixels. **GLSL only.**
|[ *vertex_shader*
:[ string
:[
:[ Path to the vertex shader. **GLSL only; set in the *[output]* section of the cava configuration file.**
|[ *fragment_shader*
:[ string
:[
:[ Path to the fragment shader. **GLSL only; set in the *[output]* section of the cava configuration file.**
|[ *continuous_rendering*
:[ integer
:[ 0
:[ Continue rendering when silent. Set to 1 for smooth animation. **GLSL only; set in the *[output]* section of the cava configuration file.**
|[ *background*
:[ string
:[
:[ Background color as a '#RRGGBB' hex string (must be quoted). **GLSL only; set in the *[color]* section of the cava configuration file.**
|[ *foreground*
:[ string
:[
:[ Foreground color as a '#RRGGBB' hex string (must be quoted). **GLSL only; set in the *[color]* section of the cava configuration file.**
|[ *gradient*
:[ integer
:[ 0
:[ Enable gradient mode (0 = off, 1 = on). **GLSL only; set in the *[color]* section of the cava configuration file.**
|[ *gradient_count*
:[ integer
:[ 0
:[ Number of gradient colors (up to 8). **GLSL only; set in the *[color]* section of the cava configuration file.**
|[ *gradient_color_N*
:[ string
:[
:[ Gradient color N (1–8) as a '#RRGGBB' hex string (must be quoted). **GLSL only; set in the *[color]* section of the cava configuration file.**

Configuration can be provided in three ways:

- **Cava config only**: set *cava_config* to a cava configuration file and omit all other options.
- **Waybar JSON only**: leave out *cava_config* and set every option in Waybar's module configuration.
- **Mixed**: provide a *cava_config* and also set specific options in Waybar's JSON. Waybar reads the file first, then overrides any values present in the JSON.

# ACTIONS

[- *String*
:[ *Action*
|[ *mode*
:[ Toggle pause/resume for the audio capture and output threads.

# DEPENDENCIES

- iniparser
- fftw3
- epoxy (GLSL frontend only)

# SOLVING ISSUES

. At startup Waybar fails with *"error while loading shared libraries: libcava.so: cannot open shared object file: No such file or directory"*.
  This happens when libcava has not been registered in the system library cache. Run *sudo ldconfig* to refresh the cache.
  This can also occur when Waybar is installed under */usr/local* but libcava is elsewhere. To fix it:
   1. Remove the local libcava installation: *sudo rm -rfv /usr/local/include/cava /usr/local/lib64/pkgconfig/cava.pc /usr/local/lib64/libcava.so*
   2. Reconfigure Waybar to use the system prefix: *meson configure build -Dprefix="/usr"*
   3. Rebuild Waybar: *ninja -C build*
   4. Install Waybar: *sudo meson install -C build*

. Waybar starts but the cava module does not react to audio.
   1. First, verify that standalone cava works correctly.
   2. If it does, comment out all Waybar cava options, uncomment *cava_config*, and point it to the working cava configuration file.
   3. The *input_delay* may be too large or too small. Try setting it to 4 seconds, restart Waybar, and check again after that delay. This is usually sufficient, even on slower machines.
   4. You may have accidentally toggled pause mode via an action.

# REPORTING ISSUES

This module is a consumer of the cava API. For bugs in the cava engine itself, please report them to [cava upstream](https://github.com/karlstav/cava) first.

Upstream cava does not provide a shared library. The Waybar cava module uses [libcava](https://github.com/LukashonakV/cava), a fork maintained by the module author, to provide one. If the issue is specific to the shared library packaging, report it to libcava.

When requesting a new upstream cava release to be packaged in libcava, open an issue at libcava with the title `[Bump] x.x.x`, where `x.x.x` is the desired cava version.

# EXAMPLES

## Raw frontend

```
"cava": {
	//"cava_config": "$XDG_CONFIG_HOME/cava/cava.conf",
	"framerate": 30,
	"autosens": 1,
	//"sensitivity": 100,
	"bars": 14,
	"lower_cutoff_freq": 50,
	"higher_cutoff_freq": 10000,
	"method": "pulse",
	"source": "auto",
	"stereo": true,
	"reverse": false,
	"monstercat": false,
	"waves": false,
	"noise_reduction": 0.77,
	"input_delay": 2,
	"format-icons" : ["▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" ],
	"actions": {
		"on-click-right": "mode"
	}
},
```

waybar_raw.conf
```
## Configuration file for CAVA.
# Remove the ; to change parameters.


[general]

# Smoothing mode. Can be 'normal', 'scientific' or 'waves'. DEPRECATED as of 0.6.0

# Accepts only non-negative values.

# 'autosens' will attempt to decrease sensitivity if the bars peak. 1 = on, 0 = off
# new as of 0.6.0 autosens of low values (dynamic range)
# 'overshoot' allows bars to overshoot (in % of terminal height) without initiating autosens. DEPRECATED as of 0.6.0

# Manual sensitivity in %. If autosens is enabled, this will only be the initial value.
# 200 means double height. Accepts only non-negative values.

# The number of bars (0-512). 0 sets it to auto (fill up console).
# Bars' width and space between bars in number of characters.
bars = 12
# bar_height is only used for output in "noritake" format

# For SDL width and space between bars is in pixels, defaults are:

# sdl_glsl have these default values, they are only used to calculate max number of bars.


# Lower and higher cutoff frequencies for lowest and highest bars
# the bandwidth of the visualizer.
# Note: there is a minimum total bandwidth of 43Mhz x number of bars.
# Cava will automatically increase the higher cutoff if a too low band is specified.

# Seconds with no input before cava goes to sleep mode. Cava will not perform FFT or drawing and
# only check for input once per second. Cava will wake up once input is detected. 0 = disable.
sleep_timer = 5


[input]

# Audio capturing method. Possible methods are: 'fifo', 'portaudio', 'pipewire', 'alsa', 'pulse', 'sndio', 'oss', 'jack' or 'shmem'
# Defaults to 'oss', 'pipewire', 'sndio', 'jack', 'pulse', 'alsa', 'portaudio' or 'fifo', in that order, dependent on what support cava was built with.
# On Mac it defaults to 'portaudio' or 'fifo'
# On windows this is automatic and no input settings are needed.
#
# All input methods uses the same config variable 'source'
# to define where it should get the audio.
#
# For pulseaudio and pipewire 'source' will be the source. Default: 'auto', which uses the monitor source of the default sink
# (all pulseaudio sinks(outputs) have 'monitor' sources(inputs) associated with them).
#
# For pipewire 'source' will be the object name or object.serial of the device to capture from.
# Both input and output devices are supported.
#
# For alsa 'source' will be the capture device.
# For fifo 'source' will be the path to fifo-file.
# For shmem 'source' will be /squeezelite-AA:BB:CC:DD:EE:FF where 'AA:BB:CC:DD:EE:FF' will be squeezelite's MAC address
#
# For sndio 'source' will be a raw recording audio descriptor or a monitoring sub-device, e.g. 'rsnd/2' or 'snd/1'. Default: 'default'.
# README.md contains further information on how to setup CAVA for sndio.
#
# For oss 'source' will be the path to a audio device, e.g. '/dev/dsp2'. Default: '/dev/dsp', i.e. the default audio device.
# README.md contains further information on how to setup CAVA for OSS on FreeBSD.
#
# For jack 'source' will be the name of the JACK server to connect to, e.g. 'foobar'. Default: 'default'.
# README.md contains further information on how to setup CAVA for JACK.
#


# The options 'sample_rate', 'sample_bits', 'channels' and 'autoconnect' can be configured for some input methods:
#   sample_rate: fifo, pipewire, sndio, oss
#   sample_bits: fifo, pipewire, sndio, oss
#   channels:    sndio, oss, jack
#   autoconnect: jack
# Other methods ignore these settings.
#
# For 'sndio' and 'oss' they are only preferred values, i.e. if the values are not supported
# by the chosen audio device, the device will use other supported values instead.
# Example: 48000, 32 and 2, but the device only supports 44100, 16 and 1, then it
# will use 44100, 16 and 1.
#


[output]

# Output method. Can be 'ncurses', 'noncurses', 'raw', 'noritake', 'sdl'
# or 'sdl_glsl'.
# 'noncurses' (default) uses a buffer and cursor movements to only print
# changes from frame to frame in the terminal. Uses less resources and is less
# prone to tearing (vsync issues) than 'ncurses'.
#
# 'raw' is an 8 or 16 bit (configurable via the 'bit_format' option) data
# stream of the bar heights that can be used to send to other applications.
# 'raw' defaults to 200 bars, which can be adjusted in the 'bars' option above.
#
# 'noritake' outputs a bitmap in the format expected by a Noritake VFD display
#  in graphic mode. It only support the 3000 series graphical VFDs for now.
#
# 'sdl' uses the Simple DirectMedia Layer to render in a graphical context.
# 'sdl_glsl' uses SDL to create an OpenGL context. Write your own shaders or
# use one of the predefined ones.
method = raw

# Orientation of the visualization. Can be 'bottom', 'top', 'left', 'right' or
# 'horizontal'. Default is 'bottom'. 'left and 'right' are only supported on sdl
# and ncruses output. 'horizontal' (bars go up and down from center) is only supported
# on noncurses output.
# Note: many fonts have weird or missing glyphs for characters used in orientations
# other than 'bottom', which can make output not look right.

# Visual channels. Can be 'stereo' or 'mono'.
# 'stereo' mirrors both channels with low frequencies in center.
# 'mono' outputs left to right lowest to highest frequencies.
# 'mono_option' set mono to either take input from 'left', 'right' or 'average'.
# set 'reverse' to 1 to display frequencies the other way around.

# Raw output target. A fifo will be created if target does not exist.
raw_target = /dev/stdout

# Raw data format. Can be 'binary' or 'ascii'.
data_format = ascii

# Binary bit format, can be '8bit' (0-255) or '16bit' (0-65530).

# Ascii max value. In 'ascii' mode range will run from 0 to value specified here

# Ascii delimiters. In ascii format each bar and frame is separated by a delimiters.
# Use decimal value in ascii table (i.e. 59 = ';' and 10 = '\n' (line feed)).
bar_delimiter = 0

# sdl window size and position. -1,-1 is centered.

# set label on bars on the x-axis. Can be 'frequency' or 'none'. Default: 'none'
# 'frequency' displays the lower cut off frequency of the bar above.
# Only supported on ncurses and noncurses output.

# enable synchronized sync. 1 = on, 0 = off
# removes flickering in alacritty terminal emulator.
# defaults to off since the behaviour in other terminal emulators is unknown

# Shaders for sdl_glsl, located in $HOME/.config/cava/shaders

; for glsl output mode, keep rendering even if no audio

# disable console blank (screen saver) in tty
# (Not supported on FreeBSD)

# show a flat bar at the bottom of the screen when idle, 1 = on, 0 = off

# show waveform instead of frequency spectrum, 1 = on, 0 = off

[color]

# Colors can be one of seven predefined: black, blue, cyan, green, magenta, red, white, yellow.
# Or defined by hex code '#xxxxxx' (hex code must be within ''). User defined colors requires
# a terminal that can change color definitions such as Gnome-terminal or rxvt.
# default is to keep current terminal color

# SDL and sdl_glsl only support hex code colors, these are the default:

# Gradient mode, only hex defined colors are supported,
# background must also be defined in hex or remain commented out. 1 = on, 0 = off.
# You can define as many as 8 different colors. They range from bottom to top of screen

[smoothing]

# Disables or enables the so-called "Monstercat smoothing" with or without "waves". Set to 0 to disable.

# Noise reduction, int 0 - 100. default 77
# the raw visualization is very noisy, this factor adjusts the integral and gravity filters to keep the signal smooth
# 100 will be very slow and smooth, 0 will be fast and noisy.


[eq]

# This one is tricky. You can have as much keys as you want.
# Remember to uncomment more than one key! More keys = more precision.
# Look at readme.md on github for further explanations and examples.
```

## GLSL frontend

The GLSL frontend requires:

. The *epoxy* library.
. Vertex and fragment shaders from the cava project. Download them and place under _$XDG_CONFIG_HOME/cava/shaders_, then reference them in the cava configuration:
  1. [cava shaders](https://github.com/karlstav/cava/tree/master/output/shaders)
  2. [libcava shaders](https://github.com/LukashonakV/cava/tree/master/output/shaders)
. A separate cava configuration file is highly recommended.

Key cava configuration options for GLSL:

. *bars* — higher values produce more detailed visualization.
. *method* in *[output]* must be set to *sdl_glsl*.
. *sdl_width* and *sdl_height* control the module size.
. *vertex_shader* and *fragment_shader* point to the shader files under _$HOME/.config/cava/shaders_.
. *continuous_rendering* — set to 1 for smooth animation.
. *background*, *foreground*, and *gradient_color_N* must use hex codes inside single quotes.

Example:

waybar config
```
"cava": {
        "cava_config": "$XDG_CONFIG_HOME/cava/waybar_cava.conf",
        "input_delay": 2,
        "actions": {
                   "on-click-right": "mode"
                   }
        },
```

waybar_cava.conf
```
## Configuration file for CAVA.
# Remove the ; to change parameters.


[general]

# Smoothing mode. Can be 'normal', 'scientific' or 'waves'. DEPRECATED as of 0.6.0

# Accepts only non-negative values.

# 'autosens' will attempt to decrease sensitivity if the bars peak. 1 = on, 0 = off
# new as of 0.6.0 autosens of low values (dynamic range)
# 'overshoot' allows bars to overshoot (in % of terminal height) without initiating autosens. DEPRECATED as of 0.6.0

# Manual sensitivity in %. If autosens is enabled, this will only be the initial value.
# 200 means double height. Accepts only non-negative values.

# The number of bars (0-512). 0 sets it to auto (fill up console).
# Bars' width and space between bars in number of characters.
bars = 50

# bar_height is only used for output in "noritake" format

# For SDL width and space between bars is in pixels, defaults are:

# sdl_glsl have these default values, they are only used to calculate max number of bars.

# Lower and higher cutoff frequencies for lowest and highest bars
# the bandwidth of the visualizer.
# Note: there is a minimum total bandwidth of 43Mhz x number of bars.
# Cava will automatically increase the higher cutoff if a too low band is specified.

# Seconds with no input before cava goes to sleep mode. Cava will not perform FFT or drawing and
# only check for input once per second. Cava will wake up once input is detected. 0 = disable.
sleep_timer = 5


[input]

# Audio capturing method. Possible methods are: 'fifo', 'portaudio', 'pipewire', 'alsa', 'pulse', 'sndio', 'oss', 'jack' or 'shmem'
# Defaults to 'oss', 'pipewire', 'sndio', 'jack', 'pulse', 'alsa', 'portaudio' or 'fifo', in that order, dependent on what support cava was built with.
# On Mac it defaults to 'portaudio' or 'fifo'
# On windows this is automatic and no input settings are needed.
#
# All input methods uses the same config variable 'source'
# to define where it should get the audio.
#
# For pulseaudio and pipewire 'source' will be the source. Default: 'auto', which uses the monitor source of the default sink
# (all pulseaudio sinks(outputs) have 'monitor' sources(inputs) associated with them).
#
# For pipewire 'source' will be the object name or object.serial of the device to capture from.
# Both input and output devices are supported.
#
# For alsa 'source' will be the capture device.
# For fifo 'source' will be the path to fifo-file.
# For shmem 'source' will be /squeezelite-AA:BB:CC:DD:EE:FF where 'AA:BB:CC:DD:EE:FF' will be squeezelite's MAC address
#
# For sndio 'source' will be a raw recording audio descriptor or a monitoring sub-device, e.g. 'rsnd/2' or 'snd/1'. Default: 'default'.
# README.md contains further information on how to setup CAVA for sndio.
#
# For oss 'source' will be the path to a audio device, e.g. '/dev/dsp2'. Default: '/dev/dsp', i.e. the default audio device.
# README.md contains further information on how to setup CAVA for OSS on FreeBSD.
#
# For jack 'source' will be the name of the JACK server to connect to, e.g. 'foobar'. Default: 'default'.
# README.md contains further information on how to setup CAVA for JACK.
#


# The options 'sample_rate', 'sample_bits', 'channels' and 'autoconnect' can be configured for some input methods:
#   sample_rate: fifo, pipewire, sndio, oss
#   sample_bits: fifo, pipewire, sndio, oss
#   channels:    sndio, oss, jack
#   autoconnect: jack
# Other methods ignore these settings.
#
# For 'sndio' and 'oss' they are only preferred values, i.e. if the values are not supported
# by the chosen audio device, the device will use other supported values instead.
# Example: 48000, 32 and 2, but the device only supports 44100, 16 and 1, then it
# will use 44100, 16 and 1.
#


[output]

# Output method. Can be 'ncurses', 'noncurses', 'raw', 'noritake', 'sdl'
# or 'sdl_glsl'.
# 'noncurses' (default) uses a buffer and cursor movements to only print
# changes from frame to frame in the terminal. Uses less resources and is less
# prone to tearing (vsync issues) than 'ncurses'.
#
# 'raw' is an 8 or 16 bit (configurable via the 'bit_format' option) data
# stream of the bar heights that can be used to send to other applications.
# 'raw' defaults to 200 bars, which can be adjusted in the 'bars' option above.
#
# 'noritake' outputs a bitmap in the format expected by a Noritake VFD display
#  in graphic mode. It only support the 3000 series graphical VFDs for now.
#
# 'sdl' uses the Simple DirectMedia Layer to render in a graphical context.
# 'sdl_glsl' uses SDL to create an OpenGL context. Write your own shaders or
# use one of the predefined ones.
method = sdl_glsl

# Orientation of the visualization. Can be 'bottom', 'top', 'left', 'right' or
# 'horizontal'. Default is 'bottom'. 'left and 'right' are only supported on sdl
# and ncruses output. 'horizontal' (bars go up and down from center) is only supported
# on noncurses output.
# Note: many fonts have weird or missing glyphs for characters used in orientations
# other than 'bottom', which can make output not look right.

# Visual channels. Can be 'stereo' or 'mono'.
# 'stereo' mirrors both channels with low frequencies in center.
# 'mono' outputs left to right lowest to highest frequencies.
# 'mono_option' set mono to either take input from 'left', 'right' or 'average'.
# set 'reverse' to 1 to display frequencies the other way around.

# Raw output target. A fifo will be created if target does not exist.

# Raw data format. Can be 'binary' or 'ascii'.

# Binary bit format, can be '8bit' (0-255) or '16bit' (0-65530).

# Ascii max value. In 'ascii' mode range will run from 0 to value specified here

# Ascii delimiters. In ascii format each bar and frame is separated by a delimiters.
# Use decimal value in ascii table (i.e. 59 = ';' and 10 = '\n' (line feed)).
bar_delimiter = 0

# sdl window size and position. -1,-1 is centered.
sdl_width = 150
sdl_height = 39

# set label on bars on the x-axis. Can be 'frequency' or 'none'. Default: 'none'
# 'frequency' displays the lower cut off frequency of the bar above.
# Only supported on ncurses and noncurses output.

# enable synchronized sync. 1 = on, 0 = off
# removes flickering in alacritty terminal emulator.
# defaults to off since the behaviour in other terminal emulators is unknown

# Shaders for sdl_glsl, located in $HOME/.config/cava/shaders
vertex_shader = pass_through.vert
fragment_shader = bar_spectrum.frag

; for glsl output mode, keep rendering even if no audio
continuous_rendering = 1;

# disable console blank (screen saver) in tty
# (Not supported on FreeBSD)

# show a flat bar at the bottom of the screen when idle, 1 = on, 0 = off

# show waveform instead of frequency spectrum, 1 = on, 0 = off

[color]


# Colors can be one of seven predefined: black, blue, cyan, green, magenta, red, white, yellow.
# Or defined by hex code '#xxxxxx' (hex code must be within ''). User defined colors requires
# a terminal that can change color definitions such as Gnome-terminal or rxvt.
# default is to keep current terminal color

# SDL and sdl_glsl only support hex code colors, these are the default:
background = '#282C34'

# Gradient mode, only hex defined colors are supported,
# background must also be defined in hex or remain commented out. 1 = on, 0 = off.
# You can define as many as 8 different colors. They range from bottom to top of screen
gradient = 1
gradient_count = 2
gradient_color_1 = '#282C34'
gradient_color_2 = '#45475A'

; gradient_color_1 = '#59cc33'
; gradient_color_2 = '#80cc33'
 gradient_color_3 = '#a6cc33'
 gradient_color_4 = '#cccc33'
 gradient_color_5 = '#cca633'
 gradient_color_6 = '#cc8033'
 gradient_color_7 = '#cc5933'
 gradient_color_8 = '#cc3333'

[smoothing]

# Percentage value for integral smoothing. Takes values from 0 - 100.
# Higher values means smoother, but less precise. 0 to disable.
# DEPRECATED as of 0.8.0, use noise_reduction instead

# Disables or enables the so-called "Monstercat smoothing" with or without "waves". Set to 0 to disable.

# Set gravity percentage for "drop off". Higher values means bars will drop faster.
# Accepts only non-negative values. 50 means half gravity, 200 means double. Set to 0 to disable "drop off".
# DEPRECATED as of 0.8.0, use noise_reduction instead


# In bar height, bars that would have been lower that this will not be drawn.
# DEPRECATED as of 0.8.0

# Noise reduction, int 0 - 100. default 77
# the raw visualization is very noisy, this factor adjusts the integral and gravity filters to keep the signal smooth
# 100 will be very slow and smooth, 0 will be fast but noisy.

[eq]

# This one is tricky. You can have as much keys as you want.
# Remember to uncomment more than one key! More keys = more precision.
# Look at readme.md on github for further explanations and examples.
```

More GLSL examples are available on the [cava GLSL wiki page](https://github.com/Alexays/Waybar/wiki/Module:-Cava:-GLSL).

# STYLE

- *#cava* Raw frontend widget
- *#cava.silent* Applied after no sound has been detected for *sleep_timer* seconds
- *#cava.updated* Applied when a new frame is shown
- *#cavaGLSL* GLSL frontend widget (used instead of *#cava* when the cava *method* is *sdl_glsl*)
- *#cavaGLSL.silent* Applied after no sound has been detected for *sleep_timer* seconds
- *#cavaGLSL.updated* Applied when a new frame is shown

# FRONTENDS

## RAW
The raw frontend maps each bar's amplitude to a character from *format-icons*. The final widget text is the concatenation of all characters, optionally separated by *bar_delimiter*. See the EXAMPLES section above for a complete configuration.

## GLSL
The GLSL frontend renders the visualization with OpenGL ES using user-provided shaders. It is selected by setting *method = sdl_glsl* in the cava configuration. See the EXAMPLES section above for a complete configuration.
