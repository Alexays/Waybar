waybar-cava(5) "waybar-cava" "User Manual"

# NAME

waybar - cava module

# DESCRIPTION

*cava* module for karlstav/cava project. See it on github: https://github.com/karlstav/cava.


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
:[ Manual sensitivity in %. It's recommended to be omitted when *autosens* = 1
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
|[ *method*
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
:[ double
:[ 0.77
:[ Range between 0 - 1. The raw visualization is very noisy, this factor adjusts the integral and gravity filters to keep the signal smooth. 1 - will be very slow and smooth, 0 - will be fast but noisy
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
:[ It's impossible to set it. Waybar sets it to = asci for internal needs
|[ *raw_target*
:[ string
:[ /dev/stdout
:[ It's impossible to set it. Waybar sets it to = /dev/stdout for internal needs
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
