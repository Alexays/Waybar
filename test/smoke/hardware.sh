#!/usr/bin/env bash
# Tier 6 - fake hardware via umockdev. backlight, backlight/slider (inside a
# group, the #5179 shape), battery and temperature read sysfs/udev, which a
# headless runner does not have. umockdev-run feeds waybar a mock sysfs tree;
# the script then rewrites attributes in that tree and asserts the exact text
# each module shows, through the AT-SPI tree.
#
# Usage: dbus-run-session -- hardware.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

export SMOKE_A11Y=1
smoke::setup
# umockdev-run's LD_PRELOAD lands ahead of the ASan runtime; without this ASan
# aborts before main() ("ASan runtime does not come first").
export ASAN_OPTIONS="${ASAN_OPTIONS}:verify_asan_link_order=0"
# The backlight backend needs a system bus to build its logind proxy (it is only
# called on scroll), or the module disables itself. The session bus stands in.
export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
smoke::start_compositor
trap '[ $? = 0 ] || { echo "::group::waybar log"; smoke::log; echo "::endgroup::"; }; smoke::stop' EXIT

WORK="$(mktemp -d)"
# The battery is listed only with uevent and capacity files; its percentage and
# time estimate come from energy_now, energy_full and power_now.
cat > "$WORK/devices.umockdev" <<'EOF'
P: /devices/platform/fake_backlight/backlight/intel_backlight
E: SUBSYSTEM=backlight
A: brightness=500
A: max_brightness=1000
A: actual_brightness=500
A: bl_power=0

P: /devices/platform/fake_battery/power_supply/BAT0
E: SUBSYSTEM=power_supply
E: POWER_SUPPLY_NAME=BAT0
A: uevent=POWER_SUPPLY_NAME=BAT0
A: type=Battery
A: present=1
A: status=Discharging
A: capacity=55
A: energy_now=5500000
A: energy_full=10000000
A: power_now=1000000

P: /devices/virtual/hwmon/hwmon7
E: SUBSYSTEM=hwmon
A: name=smoke_temp
A: temp1_input=42000
EOF

cat > "$WORK/config.json" <<'EOF'
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["battery", "backlight", "group/bl", "temperature"],
  "group/bl": { "modules": ["backlight/slider"] },
  "battery": { "interval": 1, "format": "BAT {capacity}% {time}",
               "format-charging": "CHG {capacity}% {time}", "format-full": "FULL {capacity}%" },
  "backlight": { "format": "LIGHT {percent}%" },
  "backlight/slider": { "min": 0, "max": 100, "orientation": "horizontal" },
  "temperature": { "interval": 1, "hwmon-name": "smoke_temp", "input-filename": "temp1_input",
                   "critical-threshold": 80, "format": "TEMP {temperatureC}C",
                   "format-critical": "HOT {temperatureC}C" } }
EOF

# umockdev-run builds its testbed in $TMPDIR/umockdev.XXXXXX.
mkdir "$WORK/tmp"
WAYBAR_WRAP="env TMPDIR=$WORK/tmp umockdev-run --device $WORK/devices.umockdev --" \
    smoke::launch_waybar "$WORK/config.json" "$DIR/style.css" 1
smoke::assert_alive
SYS="$(echo "$WORK"/tmp/umockdev.*/sys/devices)"
[ -d "$SYS" ] || { echo "::error::umockdev testbed not found under $WORK/tmp"; exit 1; }

# set_attr <device path under /sys/devices> <attribute=value>...
# Each file is replaced by a rename so the module never reads a half-written value.
set_attr() {
    local dev="$SYS/$1" kv; shift
    for kv in "$@"; do
        printf '%s\n' "${kv#*=}" > "$dev/.${kv%%=*}.new"
        mv -f "$dev/.${kv%%=*}.new" "$dev/${kv%%=*}"
    done
}
BAT=platform/fake_battery/power_supply/BAT0
BL=platform/fake_backlight/backlight/intel_backlight
TEMP=virtual/hwmon/hwmon7

# A failed assertion ends the tier (set -e); the EXIT trap tears down.
A11Y=(python3 "$DIR/a11y.py" --timeout 8)

echo "::group::initial values"
# 5.5 Wh left at 1 W: 5 h 30 min to empty.
"${A11Y[@]}" "BAT 55% 5 h 30 min" "LIGHT 50%" "value=50" "TEMP 42C"
echo "::endgroup::"

echo "::group::battery: charging, then full"
# 3 Wh missing at 2 W: 1 h 30 min to full.
set_attr "$BAT" energy_now=7000000 power_now=2000000 status=Charging
"${A11Y[@]}" "CHG 70% 1 h 30 min"
set_attr "$BAT" energy_now=10000000 status=Full
"${A11Y[@]}" "FULL 100%"
set_attr "$BAT" energy_now=2500000 power_now=1000000 status=Discharging
"${A11Y[@]}" "BAT 25% 2 h 30 min"
echo "::endgroup::"

echo "::group::backlight: brightness change reaches label and slider"
set_attr "$BL" brightness=250 actual_brightness=250
"${A11Y[@]}" "LIGHT 25%" "value=25"
echo "::endgroup::"

echo "::group::temperature: critical threshold"
# 85.5 C rounds to 86, above the critical threshold of 80.
set_attr "$TEMP" temp1_input=85500
"${A11Y[@]}" "HOT 86C"
set_attr "$TEMP" temp1_input=79400
"${A11Y[@]}" "TEMP 79C"
echo "::endgroup::"

# umockdev-run forwards SIGINT and returns waybar's status.
smoke::assert_clean_exit
echo "✓ fake-hardware tier passed"
