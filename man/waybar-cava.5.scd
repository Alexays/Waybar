waybar-cava(5) "waybar-cava" "User Manual"

# NAME

waybar - cava module

# DESCRIPTION

*cava* module for karlstav/cava project. See it on github: https://github.com/karlstav/cava.

Module supports two different frontends starting from the 0.15.0 release. The frontend that
will be used is managed by the method parameter in the [output] section of the cava configuration file.

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
:- *Typeof*
:- *Default*
:- *Description*
|[ *cava_config*
:[ string
:[
:< Path where cava configuration file is placed to
|[ *method* \[output\]
:[ string
:[
:< Manages which frontend Waybar cava module should use. Values: raw, sdl_glsl
|[ *framerate*
:[ integer
:[ 30
:[ Frames per second. Is used as a replacement for *interval*
|[ *autosens*
:[ integer
:[ 1
:[ Will attempt to decrease sensitivity if the bars peak
|[ *sensitivity*
:[ integer
:[ 100
:[ Manual sensitivity in %. If autosens is enabled, this will only be the initial value. 200 means double height. Accepts only non-negative values
|[ *bars*
:[ integer
:[ 12
:[ The number of bars
|[ *lower_cutoff_freq*
:[ long integer
:[ 50
:[ Lower cutoff frequencies for lowest bars the bandwidth of the visualizer
|[ *higher_cutoff_freq*
:[ long integer
:[ 10000
:[ Higher cutoff frequencies for highest bars the bandwidth of the visualizer
|[ *sleep_timer*
:[ integer
:[ 5
:[ Seconds with no input before cava main thread goes to sleep mode
|[ *hide_on_silence*
:[ bool
:[ false
:[ Hides the widget if no input (after sleep_timer elapsed)
|[ *format_silent*
:[ string
:[
:[ Widget's text after sleep_timer elapsed (hide_on_silence has to be false)
|[ *method* \[input\]
:[ string
:[ pulse
:[ Audio capturing method. Possible methods are: pipewire, pulse, alsa, fifo, sndio or shmem
|[ *source*
:[ string
:[ auto
:[ See cava configuration
|[ *sample_rate*
:[ long integer
:[ 44100
:[ See cava configuration
|[ *sample_bits*
:[ integer
:[ 16
:[ See cava configuration
|[ *stereo*
:[ bool
:[ true
:[ Visual channels
|[ *reverse*
:[ bool
:[ false
:[ Displays frequencies the other way around
|[ *bar_delimiter*
:[ integer
:[ 0
:[ Each bar is separated by a delimiter. Use decimal value in ascii table(i.e. 59 = ";"). 0 means no delimiter
|[ *monstercat*
:[ bool
:[ false
:[ Disables or enables the so-called "Monstercat smoothing" with or without "waves"
|[ *waves*
:[ bool
:[ false
:[ Disables or enables the so-called "Monstercat smoothing" with or without "waves"
|[ *noise_reduction*
:[ integer
:[ 77
:[ Range between 0 - 100. The raw visualization is very noisy, this factor adjusts the integral and gravity filters to keep the signal smooth. 100 will be very slow and smooth, 0 will be fast but noisy
|[ *input_delay*
:[ integer
:[ 2
:[ Sets the delay before fetching audio source thread start working. On author's machine, Waybar starts much faster than pipewire audio server, and without a little delay cava module fails because pipewire is not ready
|[ *ascii_max_range*
:[ integer
:[ 7
:[ It's impossible to set it directly. The value is dictated by the number of icons in the array *format-icons*
|[ *data_format*
:[ string
:[ asci
:[ Raw data format. Can be 'binary' or 'ascii'
|[ *raw_target*
:[ string
:[ /dev/stdout
:[ Raw output target. A fifo will be created if target does not exist
|[ *menu*
:[ string
:[
:[ Action that popups the menu.
|[ *menu-file*
:[ string
:[
:[ Location of the menu descriptor file. There need to be an element of type GtkMenu with id *menu*
|[ *menu-actions*
:[ array
:[
:[ The actions corresponding to the buttons of the menu.
|[ *bar_spacing*
:[ integer
:[
:[ Bars' space between bars in number of characters
|[ *bar_width*
:[ integer
:[
:[ Bars' width between bars in number of characters
|[ *bar_height*
:[ integer
:[
:[ Useless. bar_height is only used for output in "noritake" format
|[ *background*
:[ string
:[
:[ GLSL actual. Support hex code colors only. Must be within ''
|[ *foreground*
:[ string
:[
:[ GLSL actual. Support hex code colors only. Must be within ''
|[ *gradient*
:[ integer
:[ 0
:[ GLSL actual. Gradient mode(0/1 - on/off)
|[ *gradient_count*
:[ integer
:[ 0
:[ GLSL actual. The count of colors for the gradient
|[ *gradient_color_N*
:[ string
:[
:[ GLSL actual. N - the number of the gradient color between 1 and 8. Only hex defined colors are supported. Must be within ''
|[ *sdl_width*
:[ integer
:[
:[ GLSL actual. Manages the width of the waybar cava GLSL frontend module
|[ *sdl_height*
:[ integer
:[
:[ GLSL actual. Manages the height of the waybar cava GLSL frontend module
|[ *continuous_rendering*
:[ integer
:[ 0
:[ GLSL actual. Keep rendering even if no audio. Recommended to set to 1

Configuration can be provided as:
- The only cava configuration file which is provided through *cava_config*. The rest configuration can be skipped
- Without cava configuration file. In such case cava should be configured through provided list of the configuration option
- Mix. When provided both And cava configuration file And configuration options. In such case, waybar applies configuration file first and then overrides particular options by the provided list of configuration options

# ACTIONS

[- *String*
:- *Action*
|[ *mode*
:< Switch main cava thread and fetch audio source thread from/to pause/resume

# DEPENDENCIES

- iniparser
- fftw3
- epoxy (GLSL frontend only)

# SOLVING ISSUES

. On start Waybar throws an exception "error while loading shared libraries: libcava.so: cannot open shared object file: No such file or directory".
  It might happen when libcava for some reason hasn't been registered in the system. sudo ldconfig should help
. Waybar is starting but cava module doesn't react to the music
   1. In such cases at first need to make sure usual cava application is working as well
   2. If so, need to comment all configuration options. Uncomment cava_config and provide the path to the working cava config
   3. You might set too huge or too small input_delay. Try to setup to 4 seconds, restart waybar, and check again 4 seconds past. Usual even on weak machines it should be enough
   4. You might accidentally switch action mode to pause mode

# RISING ISSUES

For clear understanding: this module is a cava API's consumer. So for any bugs related to cava engine you should contact Cava upstream(https://github.com/karlstav/cava) ++
with the one Exception. Cava upstream doesn't provide cava as a shared library. For that, this module author made a fork libcava(https://github.com/LukashonakV/cava). ++
So the order is:
. cava upstream
. libcava upstream.
In case when cava releases new version and you're wanna get it, it should be raised an issue to libcava(https://github.com/LukashonakV/cava) with title ++
\[Bump\]x.x.x where x.x.x is cava release version.

# EXAMPLES

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
	"bar_delimiter": 0,
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
# STYLE

- *#cava*
- *#cava.silent* Applied after no sound has been detected for sleep_timer seconds
- *#cava.updated* Applied when a new frame is shown
# FRONTENDS

## RAW
The cava raw frontend uses ASCII characters to visualize incoming audio data. Each ASCII symbol position corresponds to the value of the audio power pulse.

Under the hood:
```
. Incoming audio power pulse list is : 12684
. Configured array of ASCII codes is: ["▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" ]. See `format-icons` https://github.com/Alexays/Waybar/wiki/Module:-Cava#example 
```
As a result cava frontend will give ▁▂▆█▄

Examples:

waybar config
```
"cava": {
        "cava_config": "$XDG_CONFIG_HOME/cava/waybar_raw.conf",
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
# 100 will be very slow and smooth, 0 will be fast but noisy.


[eq]

# This one is tricky. You can have as much keys as you want.
# Remember to uncomment more than one key! More keys = more precision.
# Look at readme.md on github for further explanations and examples.
```
## GLSL
The Cava GLSL frontend delegates the visualization of incoming audio data to the GPU via OpenGL.

There are some mandatory dependencies that need to be satisfied in order for Cava GLSL to be built and function properly:

. epoxy library must be installed on the system
. Vertex and fragment shaders from the original project must be used. They should be downloaded, and the file paths must be configured correctly in the Waybar Cava configuration:
   1. cava shaders [cava shaders](https://github.com/karlstav/cava/tree/master/output/shaders)
   2. libcava shaders [libcava shaders](https://github.com/LukashonakV/cava/tree/master/output/shaders)
. It is highly recommended to have a separate cava configuration for the Waybar Cava GLSL module and to use this as the cava_config in the Waybar configuration.
. It is common for cava configurations to be placed in the XDG_CONFIG_HOME directory, including shaders as well. Consider keeping them in the $XDG_CONFIG_HOME/cava/shaders folder.

Key configuration options:

. bars. The more values the parameter has, the more interesting the visualization becomes.
. method in output section must be set to sdl_glsl
. sdl_width and sdl_height manage the size of the module. Adjust them according to your needs.
. Shaders for sdl_glsl, located in $HOME/.config/cava/shaders. Example: "vertex_shader" = "pass_through.vert" "fragment_shader" = "spectrogram.frag"
. Set continuous_rendering to 1 to enable smooth rendering; set it to 0 otherwise. It is recommended to keep it set to 1.
. background, foreground, and gradient_color_N (where N is a number between 1 and 8) must be defined using hex code

Example:

waybar config
```
"cava": {
        "cava_config": "$XDG_CONFIG_HOME/cava/waybar_cava#3.conf",
        "input_delay": 2,
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

Different waybar_cava#N.conf see at [cava GLSL](https://github.com/Alexays/Waybar/wiki/Module:-Cava:-GLSL)
