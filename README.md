# Waybar [![Travis](https://travis-ci.org/Alexays/Waybar.svg?branch=master)](https://travis-ci.org/Alexays/Waybar) [![Licence](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![Paypal Donate](https://img.shields.io/badge/Donate-Paypal-2244dd.svg)](https://paypal.me/ARouillard)<br>![Waybar](https://raw.githubusercontent.com/alexays/waybar/master/preview-2.png)

> Highly customizable Wayland bar for Sway and Wlroots based compositors.<br>
> Available in Arch [community](https://www.archlinux.org/packages/community/x86_64/waybar/) or
[AUR](https://aur.archlinux.org/packages/waybar-git/), [openSUSE](https://build.opensuse.org/package/show/X11:Wayland/waybar), and [Alpine Linux](https://pkgs.alpinelinux.org/packages?name=waybar)<br>
> *Waybar [examples](https://github.com/Alexays/Waybar/wiki/Examples)*

**Current features**
- Sway (Workspaces, Binding mode, Focused window name)
- Tray [#21](https://github.com/Alexays/Waybar/issues/21)
- Local time
- Battery
- Network
- Pulseaudio
- Disk
- Memory
- Cpu load average
- Temperature
- MPD
- Custom scripts
- Multiple output configuration
- And much more customizations

**Configuration and Styling**

[See the wiki for more details](https://github.com/Alexays/Waybar/wiki).

**How to build**

```bash
$ git clone https://github.com/Alexays/Waybar
$ cd Waybar
$ meson build
$ ninja -C build
$ ./build/waybar
# If you want to install it
$ ninja -C build install
$ waybar
```

**Dependencies**

```
gtkmm3
jsoncpp
libinput

libsigc++
fmt
wayland
wlroots
libpulse [Pulseaudio module]
libnl [Network module]
sway [Sway modules]
libdbusmenu-gtk3 [Tray module]
libmpdclient [MPD module]
```

On Ubuntu 19.10 you can install all the relevant dependencies using this command:

```
sudo apt install libgtkmm-3.0-dev libjsoncpp-dev libinput-dev libsigc++-2.0-dev libpulse-dev libnl-3-dev libdbusmenu-gtk3-dev libnl-genl-3-dev libfmt-dev clang-tidy scdoc libmpdclient-dev
```


Contributions welcome! - have fun :)<br>
The style guidelines is [Google's](https://google.github.io/styleguide/cppguide.html)

## License

Waybar is licensed under the MIT license. [See LICENSE for more information](https://github.com/Alexays/Waybar/blob/master/LICENSE).
