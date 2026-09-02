# Waybar

[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Donate](https://img.shields.io/badge/Donate-Paypal-2244dd.svg)](https://paypal.me/ARouillard)
[![CI](https://github.com/Alexays/Waybar/actions/workflows/linux.yml/badge.svg)](https://github.com/Alexays/Waybar/actions/workflows/linux.yml)
[![Release](https://img.shields.io/github/v/release/Alexays/Waybar)](https://github.com/Alexays/Waybar/releases)

![Waybar](https://raw.githubusercontent.com/alexays/waybar/master/preview-2.png)

> Highly customizable Wayland bar for Sway and wlroots-based compositors.<br>
> Available in [all major distributions](https://github.com/Alexays/Waybar/wiki/Installation).

**[Installation](#installation) · [Wiki](https://github.com/Alexays/Waybar/wiki) · [Configuration](https://github.com/Alexays/Waybar/wiki/Configuration) · [Styling](https://github.com/Alexays/Waybar/wiki/Styling) · [Examples](https://github.com/Alexays/Waybar/wiki/Examples) · [FAQ](https://github.com/Alexays/Waybar/wiki/FAQ)**

## Features

### Compositor integration

| Compositor | Workspaces / Tags | Window | Layout | Language | Mode |
| --- | :---: | :---: | :---: | :---: | :---: |
| [Sway](https://github.com/Alexays/Waybar/wiki/Module:-Sway) | ✅ | ✅ | | ✅ | ✅ |
| [River](https://github.com/Alexays/Waybar/wiki/Module:-River) | ✅ | ✅ | ✅ | | ✅ |
| [Hyprland](https://github.com/Alexays/Waybar/wiki/Module:-Hyprland) | ✅ | ✅ | | ✅ | ✅ |
| [Niri](https://github.com/Alexays/Waybar/wiki/Module:-Niri) | ✅ | ✅ | | ✅ | |
| [Mango](https://github.com/Alexays/Waybar/wiki/Module:-Mango) | ✅ | ✅ | ✅ | ✅ | ✅ |
| [DWL](https://github.com/Alexays/Waybar/wiki/Module:-Dwl) | ✅ | ✅ | | | |
| [Wayfire](https://github.com/Alexays/Waybar/wiki/Module:-Wayfire) | ✅ | ✅ | | | |

> DWL requires the [dwl IPC patch](https://codeberg.org/dwl/dwl-patches/src/branch/main/patches/ipc).

### Modules

- **Power & hardware** — [Battery](https://github.com/Alexays/Waybar/wiki/Module:-Battery), [UPower](https://github.com/Alexays/Waybar/wiki/Module:-UPower), [Power profiles daemon](https://github.com/Alexays/Waybar/wiki/Module:-PowerProfilesDaemon), [Backlight](https://github.com/Alexays/Waybar/wiki/Module:-Backlight), [CPU](https://github.com/Alexays/Waybar/wiki/Module:-CPU), [Memory](https://github.com/Alexays/Waybar/wiki/Module:-Memory), [Disk](https://github.com/Alexays/Waybar/wiki/Module:-Disk), [Temperature](https://github.com/Alexays/Waybar/wiki/Module:-Temperature)
- **Connectivity** — [Network](https://github.com/Alexays/Waybar/wiki/Module:-Network), [Bluetooth](https://github.com/Alexays/Waybar/wiki/Module:-Bluetooth), [GPS](https://github.com/Alexays/Waybar/wiki/Module:-GPS), [WWAN](https://github.com/Alexays/Waybar/wiki/Module:-WWAN)
- **Audio & media** — [PulseAudio](https://github.com/Alexays/Waybar/wiki/Module:-PulseAudio), [WirePlumber](https://github.com/Alexays/Waybar/wiki/Module:-WirePlumber), [JACK](https://github.com/Alexays/Waybar/wiki/Module:-JACK), [sndio](https://github.com/Alexays/Waybar/wiki/Module:-Sndio), [Cava](https://github.com/Alexays/Waybar/wiki/Module:-Cava), [MPD](https://github.com/Alexays/Waybar/wiki/Module:-MPD), [MPRIS](https://github.com/Alexays/Waybar/wiki/Module:-MPRIS)
- **Desktop** — [Clock & calendar](https://github.com/Alexays/Waybar/wiki/Module:-Clock), [System tray](https://github.com/Alexays/Waybar/wiki/Module:-Tray), [Idle inhibitor](https://github.com/Alexays/Waybar/wiki/Module:-Idle-Inhibitor), [Keyboard state](https://github.com/Alexays/Waybar/wiki/Module:-Keyboard-State), [Privacy](https://github.com/Alexays/Waybar/wiki/Module:-Privacy), [Gamemode](https://github.com/Alexays/Waybar/wiki/Module:-Gamemode), [Systemd failed units](https://github.com/Alexays/Waybar/wiki/Module:-Systemd-failed-units), [Image](https://github.com/Alexays/Waybar/wiki/Module:-Image), [Custom scripts](https://github.com/Alexays/Waybar/wiki/Module:-Custom)

…and more. Every module is documented on the [wiki](https://github.com/Alexays/Waybar/wiki) (see the *Modules* sidebar).

## Getting started

```bash
git clone https://github.com/Alexays/Waybar
cd Waybar
meson setup build
ninja -C build
./build/waybar          # run without installing
```

Waybar launches with a sensible [default config](resources/config.jsonc). To make
it yours, copy the default config and stylesheet into `~/.config/waybar/` and edit
them. The [Configuration](https://github.com/Alexays/Waybar/wiki/Configuration)
and [Styling](https://github.com/Alexays/Waybar/wiki/Styling) guides cover every
option, and [Examples](https://github.com/Alexays/Waybar/wiki/Examples) has
ready-to-use community setups.

## Installation

Waybar is packaged by most distributions:

[![Packaging status](https://repology.org/badge/vertical-allrepos/waybar.svg?columns=3&header=Waybar%20Downstream%20Packaging)](https://repology.org/project/waybar/versions)

An Ubuntu PPA with more recent versions is available [here](https://launchpad.net/~nschloe/+archive/ubuntu/waybar).

### Building from source

```bash
git clone https://github.com/Alexays/Waybar
cd Waybar
meson setup build
ninja -C build
ninja -C build install   # optional
```

<details>
<summary><b>Runtime dependencies</b></summary>

```
gtkmm3       jsoncpp      libsigc++    fmt          wayland
chrono-date  spdlog       xkbregistry  libgtk-3-dev upower

libpulse             [Pulseaudio module]
libnl                [Network module]
libappindicator-gtk3 [Tray module]
libdbusmenu-gtk3     [Tray module]
libmpdclient         [MPD module]
libsndio             [sndio module]
libevdev             [KeyboardState module]
```
</details>

<details>
<summary><b>Build dependencies</b></summary>

```
cmake   meson   scdoc   wayland-protocols
```
</details>

<details>
<summary><b>Install dependencies — Ubuntu</b></summary>

```bash
sudo apt install \
  clang-tidy gobject-introspection libdbusmenu-gtk3-dev libevdev-dev \
  libfmt-dev libgirepository1.0-dev libgtk-3-dev libgtkmm-3.0-dev \
  libinput-dev libjsoncpp-dev libmpdclient-dev libnl-3-dev libnl-genl-3-dev \
  libpulse-dev libsigc++-2.0-dev libspdlog-dev libwayland-dev scdoc upower \
  libxkbregistry-dev
```
</details>

<details>
<summary><b>Install dependencies — Arch</b></summary>

```bash
pacman -S --asdeps \
  gtkmm3 jsoncpp libsigc++ fmt wayland chrono-date spdlog gtk3 \
  gobject-introspection libgirepository libpulse libnl libappindicator-gtk3 \
  libdbusmenu-gtk3 libmpdclient sndio libevdev libxkbcommon upower meson \
  cmake scdoc wayland-protocols glib2-devel
```
</details>

## Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md). The style
guidelines are [Google's C++ style](https://google.github.io/styleguide/cppguide.html).

> **Docs live in the man pages.** Module documentation is written in
> [`man/`](man) (scdoc) and auto-synced to the wiki. Edit the man page, not the
> wiki — see [`.github/wiki`](.github/wiki).

> [!CAUTION]
> Distributions of Waybar are only released on the [official GitHub page](https://github.com/Alexays/Waybar).<br>
> Waybar does **not** have an official website. Do not trust any site claiming to be official.

## License

Waybar is licensed under the MIT license. [See LICENSE for details](https://github.com/Alexays/Waybar/blob/master/LICENSE).
