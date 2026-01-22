# Waybar [![Licence](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![Paypal Donate](https://img.shields.io/badge/Donate-Paypal-2244dd.svg)](https://paypal.me/ARouillard)<br>![Waybar](https://raw.githubusercontent.com/alexays/waybar/master/preview-2.png)

> Highly customizable Wayland bar for Sway and Wlroots based compositors.<br>
> Available in [all major distributions](https://github.com/Alexays/Waybar/wiki/Installation)<br>
> *Waybar [examples](https://github.com/Alexays/Waybar/wiki/Examples)*

#### Current features
- Sway (Workspaces, Binding mode, Focused window name)
- River (Mapping mode, Tags, Focused window name)
- Hyprland (Window Icons, Workspaces, Focused window name)
- Niri (Workspaces, Focused window name, Language)
- DWL (Tags, Focused window name) [requires dwl ipc patch](https://codeberg.org/dwl/dwl-patches/src/branch/main/patches/ipc)
- Tray [#21](https://github.com/Alexays/Waybar/issues/21)
- Local time
- Battery
- UPower
- Power profiles daemon
- Network
- Bluetooth
- Pulseaudio
- Privacy Info
- Wireplumber
- Disk
- Memory
- Cpu load average
- Temperature
- MPD
- Custom scripts
- Custom image
- Multiple output configuration
- And many more customizations

#### Configuration and Styling

[See the wiki for more details](https://github.com/Alexays/Waybar/wiki).

### Installation

Waybar is available from a number of Linux distributions:

[![Packaging status](https://repology.org/badge/vertical-allrepos/waybar.svg?columns=3&header=Waybar%20Downstream%20Packaging)](https://repology.org/project/waybar/versions)

An Ubuntu PPA with more recent versions is available
[here](https://launchpad.net/~nschloe/+archive/ubuntu/waybar).


#### Building from source

```bash
$ git clone https://github.com/Alexays/Waybar
$ cd Waybar
$ meson setup build
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
libsigc++
fmt
wayland
chrono-date
spdlog
libgtk-3-dev [gtk-layer-shell]
gobject-introspection [gtk-layer-shell]
libgirepository1.0-dev [gtk-layer-shell]
libpulse [Pulseaudio module]
libnl [Network module]
libappindicator-gtk3 [Tray module]
libdbusmenu-gtk3 [Tray module]
libmpdclient [MPD module]
libsndio [sndio module]
libevdev [KeyboardState module]
xkbregistry
upower [UPower battery module]
```

**Build dependencies**

```
cmake
meson
scdoc
wayland-protocols
```

On Ubuntu, you can install all the relevant dependencies using this command (tested with 19.10 and 20.04):

```
sudo apt install \
  clang-tidy \
  gobject-introspection \
  libdbusmenu-gtk3-dev \
  libevdev-dev \
  libfmt-dev \
  libgirepository1.0-dev \
  libgtk-3-dev \
  libgtkmm-3.0-dev \
  libinput-dev \
  libjsoncpp-dev \
  libmpdclient-dev \
  libnl-3-dev \
  libnl-genl-3-dev \
  libpulse-dev \
  libsigc++-2.0-dev \
  libspdlog-dev \
  libwayland-dev \
  scdoc \
  upower \
  libxkbregistry-dev
```

On Arch, you can use this command:

```
pacman -S --asdeps \
  gtkmm3 \
  jsoncpp \
  libsigc++ \
  fmt \
  wayland \
  chrono-date \
  spdlog \
  gtk3 \
  gobject-introspection \
  libgirepository \
  libpulse \
  libnl \
  libappindicator-gtk3 \
  libdbusmenu-gtk3 \
  libmpdclient \
  sndio \
  libevdev \
  libxkbcommon \
  upower \
  meson \
  cmake \
  scdoc \
  wayland-protocols \
  glib2-devel
```


Contributions welcome!<br>
Have fun :)<br>
The style guidelines are [Google's](https://google.github.io/styleguide/cppguide.html)

## License

Waybar is licensed under the MIT license. [See LICENSE for more information](https://github.com/Alexays/Waybar/blob/master/LICENSE).
