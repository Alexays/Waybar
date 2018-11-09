# Waybar [![Travis](https://travis-ci.org/Alexays/Waybar.svg?branch=master)](https://travis-ci.org/Alexays/Waybar) [![Licence](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)<br>![Waybar](https://raw.githubusercontent.com/alexays/waybar/master/preview-2.png)
**Proof of concept**

> Highly customizable Wayland bar for Sway and Wlroots based compositors.<br>
> Available on [AUR](https://aur.archlinux.org/packages/waybar-git/)

**Current features**
- Sway Workspaces
- Sway focused window name
- Tray (Beta) [#21](https://github.com/Alexays/Waybar/issues/21)
- Local time
- Battery
- Network
- Pulseaudio
- Memory
- Cpu load average
- Custom scripts
- And much more customizations

**Configuration and Customization**

[See the wiki for more details](https://github.com/Alexays/Waybar/wiki).

**How to build**

```bash
$ git clone https://github.com/Alexays/Waybar
$ meson build
$ ninja -C build
$ ./build/waybar
```

Contributions welcome! - have fun :)<br>
The style guidelines is [Google's](https://google.github.io/styleguide/cppguide.html)

## License

Waybar is licensed under the MIT license. [See LICENSE for more information](https://github.com/Alexays/Waybar/blob/master/LICENSE).
