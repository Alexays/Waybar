# Smoke / render / interaction tests

The [`smoke`](../../.github/workflows/smoke.yml) workflow builds Waybar **with
AddressSanitizer** and runs the real binary inside a **headless** compositor
(software-rendered, no GPU) to exercise it end to end. All helpers share
[`lib.sh`](lib.sh).

## What it checks

| Tier | Check | Script |
| --- | --- | --- |
| 0 | Launches, stays alive, and logs **no ASan / `Gtk-CRITICAL` / crash** — for every launch below | [`lib.sh`](lib.sh) `assert_clean` |
| — | **Coverage tripwire** — every `Factory` module must be classified (source-only, runs before the build) | [`coverage.sh`](coverage.sh) |
| 1 | **Renders** — deterministic config screenshotted with `grim`, bar strip not blank | [`run.sh`](run.sh) |
| 1 | **Per-module matrix** — each headless-safe module rendered standalone **and inside a group**, each torn down via SIGINT | [`modules.sh`](modules.sh) |
| 2 | **Golden** — screenshot vs [`reference.png`](reference.png) (fuzz 8%, 800px) | workflow |
| 2 | **Interaction** - inject pointer input through a `wlr-virtual-pointer` device; assert `on-click` side effect, `format-alt` toggle, a `click-to-reveal` drawer whose leader has its own click option, `on-scroll-*` in four directions and `smooth-scrolling-threshold`, middle / back / forward / double click commands, a `menu-file` popup and its `menu-actions`, and tooltips shown on hover and kept while the module updates | [`interact.sh`](interact.sh) / [`pointer_click.c`](pointer_click.c) |
| 2 | **Accessibility** — assert module labels via the AT-SPI tree (robust to fonts) | [`a11y.sh`](a11y.sh) / [`a11y.py`](a11y.py) |
| 2 | **Compositor modules** - real `foot` windows driven through sway IPC: `sway/workspaces` (switch, rename, empty workspace destroyed), `sway/window` (focus), `sway/mode` (enter/leave a mode), `sway/scratchpad` (move/show), `sway/language` (layout switch on a virtual keyboard), `wlr/taskbar` (windows opening/closing, under sway and labwc), `ext/workspaces` when the compositor offers ext-workspace-v1 (not sway 1.9 on Ubuntu 24.04) | [`compositor.sh`](compositor.sh) / [`virtual_keyboard.c`](virtual_keyboard.c) |
| 3 | **Layout matrix** — top / bottom / left (vertical) + HiDPI (scale 2) | [`positions.sh`](positions.sh) |
| 3 | **Second compositor** — same launch under `labwc` | workflow (`COMPOSITOR=labwc`) |
| 4 | **Lifecycle / teardown** — clean SIGINT exit, runtime **output hotplug**, toggle + reload churn, fast-interval teardown (the C++ destructor path a `kill` never runs, #5182) | [`lifecycle.sh`](lifecycle.sh) |
| 4 | **Custom-backend fuzz** — empty / non-zero-exit / invalid-JSON / huge / non-UTF8 / empty-format exec output | [`fuzz.sh`](fuzz.sh) |
| 5 | **State transitions** - real `mpd` driven from an **empty queue** (#5183) through play / next / pause / stop / clear / server exit, `pulseaudio` + slider on a null sink through volume and mute changes; each state's text asserted over AT-SPI | [`state.sh`](state.sh) |
| 6 | **Fake hardware** - `battery` (discharging / charging / full, time estimates), `backlight`, `backlight/slider` in a group (#5179) and `temperature` (critical threshold) on a **umockdev** sysfs tree whose attributes change mid-run; exact text asserted over AT-SPI | [`hardware.sh`](hardware.sh) |
| 6 | **Fake /proc** - `cpu` (usage from two `/proc/stat` snapshots), `memory`, `cpu_frequency` and `disk` under **bubblewrap** with fake `/proc` files, a fake `/sys/devices/system/cpu` and a 16 MiB tmpfs disk; exact values asserted (`load` only renders: `getloadavg()` uses `sysinfo(2)`) | [`procfs.sh`](procfs.sh) |
| 5 | **D-Bus services** - `upower`, `bluetooth`, `power-profiles-daemon` (incl. a click cycling the profile), `systemd-failed-units`, `wwan`, `inhibitor` (click takes and drops a logind lock), `gamemode` and `mpris` (incl. play/pause click) against python-dbusmock fakes on a private system bus; each service also vanishes and returns | [`dbus.sh`](dbus.sh) |
| 4 | **Config fuzz** - every documented option of each headless-startable module (and group drawer options) set to wrong JSON types, edge numbers, deep nesting and malformed `{format}` strings, about 400 instances per bar; a failing bar is re-run per option | [`config_fuzz.sh`](config_fuzz.sh) / [`config_fuzz.py`](config_fuzz.py) |
| - | **Leak check** - clean-exit run under LSan; any leak outside the third-party [`lsan.supp`](lsan.supp) fails | [`leakcheck.sh`](leakcheck.sh) |
| 4 | **Soak** (own job, no sanitizer) - five minutes of fast modules under reload / toggle / realtime-signal / output-hotplug churn; RSS, descriptors, threads and child processes must not grow from the first to the last minute | [`soak.sh`](soak.sh) |
| 4 | **Data races** (own job) - the module, lifecycle and a short soak tier under ThreadSanitizer, with third-party entries only in [`tsan.supp`](tsan.supp) and [`tsan_signal.c`](tsan_signal.c) preloaded to keep glibc's `signal()` semantics | workflow `smoke-tsan` |
| 2 | **Compositor IPC modules** - `hyprland/*`, `niri/*`, `mango/*`, `umbriel/*` and `wayfire/*` against fake IPC servers replaying an ordinary session: workspace switch, create and (Hyprland) destroy, window open / retitle / close, keyboard layout, submap / keymode / layout; the bar keeps its state and exits cleanly once the compositor goes away. `river/*`, `dwl/*` speak Wayland protocols and are not covered | [`ipc.sh`](ipc.sh) / [`fake_ipc.py`](fake_ipc.py) |
| 4 | **Tray hostile input** - oversized `RegisterStatusNotifierItem` call, and an item whose dbusmenu layout has 20000 entries; the bar must refuse both and keep answering | [`tray.sh`](tray.sh) / [`tray_item.c`](tray_item.c) |

AddressSanitizer (plus `-D_GLIBCXX_ASSERTIONS`) runs under *all* of the above, so
any memory bug in the real render/teardown path aborts the run. The headless backend has no input device, so the
interaction step creates a virtual pointer to give the seat one.

Because the teardown path (`~Bar` → window unmap → `toggleSuspend`) is where the
exit-time crashes live, tests assert on a **clean SIGINT exit** via
`smoke::assert_clean_exit` rather than killing with SIGTERM, and re-check the log
for sanitizer reports emitted *during* destruction.

## Updating the reference image

`reference.png` must be generated by the CI environment (local rendering differs
from Ubuntu's). Two ways:

- **One click:** Actions → *smoke* → **Run workflow** with `bless = true`. The
  job regenerates the screenshot and commits it to your branch.
- **Manual:** download the `waybar-smoke` artifact from a run and commit its
  `bar.png` as `test/smoke/reference.png`.

Do this whenever an **intended** visual change trips the golden comparison.

## Run locally

On a machine with `sway`, `grim`, `imagemagick`, `jq` and Waybar built:

```sh
export WAYBAR_BIN=$PWD/build/waybar
./test/smoke/coverage.sh                     # no compositor needed
./test/smoke/run.sh       test/smoke/config.jsonc test/smoke/style.css /tmp/bar.png
./test/smoke/modules.sh   /tmp/module-shots
./test/smoke/lifecycle.sh
./test/smoke/fuzz.sh
dbus-run-session -- ./test/smoke/config_fuzz.sh
dbus-run-session -- ./test/smoke/interact.sh /tmp/interaction-shots
./test/smoke/positions.sh /tmp/layout-shots
dbus-run-session -- ./test/smoke/a11y.sh
dbus-run-session -- ./test/smoke/compositor.sh   # also needs foot, wayland-info (optional)
dbus-run-session -- ./test/smoke/tray.sh
dbus-run-session -- ./test/smoke/ipc.sh
dbus-run-session -- ./test/smoke/dbus.sh     # needs python3-dbusmock
# need extra daemons / tools:
dbus-run-session -- ./test/smoke/state.sh    # needs mpd + mpc, pulseaudio
dbus-run-session -- ./test/smoke/hardware.sh # needs umockdev
dbus-run-session -- ./test/smoke/procfs.sh   # needs bwrap and unprivileged user namespaces
./test/smoke/leakcheck.sh                    # needs an ASan build
./test/smoke/soak.sh                         # about 5 minutes
```
