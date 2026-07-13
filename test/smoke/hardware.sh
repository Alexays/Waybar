#!/usr/bin/env bash
# Tier — fake-hardware coverage via umockdev. Modules that read sysfs/udev
# (backlight, its slider, battery) can't run in a headless runner, so they were
# never smoke-tested and a construct/update crash like #5179 (backlight/slider)
# could ship unseen. umockdev-run intercepts sysfs/udev and feeds waybar a mock
# device, so the real code path runs under ASan.
#
# Best-effort: skipped if umockdev isn't installed. The .umockdev descriptions
# below may need tuning per distro; keep this continue-on-error in CI.
#
# Usage: hardware.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

if ! command -v umockdev-run >/dev/null; then
    echo "(umockdev not installed, skipping hardware tier)"; exit 0
fi

# Kill only waybar between cases -- smoke::stop would `swaymsg exit` the shared
# compositor, and the next case couldn't open the display (compositor teardown is
# the EXIT trap's job).
kill_waybar() { kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1; WAYBAR_PID=""; }

smoke::setup
# waybar is linked with ASan, but umockdev-run injects its own LD_PRELOAD, which
# lands ahead of the ASan runtime and makes ASan abort before main() ("ASan
# runtime does not come first"). Disabling the link-order check lets the
# instrumented binary run under the preload.
export ASAN_OPTIONS="${ASAN_OPTIONS}:verify_asan_link_order=0"
smoke::start_compositor
trap smoke::stop EXIT

fail=0

BL_DEV="$(mktemp --suffix=.umockdev)"
cat > "$BL_DEV" <<'EOF'
P: /devices/platform/fake_backlight/backlight/intel_backlight
E: SUBSYSTEM=backlight
A: brightness=500
A: max_brightness=1000
A: actual_brightness=500
A: bl_power=0
EOF

BAT_DEV="$(mktemp --suffix=.umockdev)"
cat > "$BAT_DEV" <<'EOF'
P: /devices/platform/fake_battery/power_supply/BAT0
E: SUBSYSTEM=power_supply
E: POWER_SUPPLY_NAME=BAT0
A: type=Battery
A: present=1
A: status=Discharging
A: capacity=55
A: energy_now=5500000
A: energy_full=10000000
A: power_now=1000000
EOF

# run_mock <name> <umockdev-file> <config-json>
run_mock() {
    local name="$1" dev="$2" mod="$3" cfg
    echo "::group::hardware: $name"
    cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<EOF
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["$name"], $mod }
EOF
    WAYBAR_WRAP="umockdev-run --device $dev --" smoke::launch_waybar "$cfg" "$DIR/style.css" 5
    smoke::assert_alive || fail=1
    smoke::assert_clean || fail=1
    kill_waybar
    echo "::endgroup::"
}

run_mock "backlight"        "$BL_DEV"  '"backlight": {"format":"{percent}%"}'
run_mock "backlight/slider" "$BL_DEV"  '"backlight/slider": {"min":0,"max":100,"orientation":"horizontal"}'
run_mock "battery"          "$BAT_DEV" '"battery": {"format":"{capacity}%"}'

# Slider inside a group -- the exact shape from #5179 (box#backlight-group).
echo "::group::hardware: backlight/slider in a group"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["group/bl"],
  "group/bl": { "modules": ["backlight/slider"] },
  "backlight/slider": { "min": 0, "max": 100, "orientation": "horizontal" } }
EOF
WAYBAR_WRAP="umockdev-run --device $BL_DEV --" smoke::launch_waybar "$cfg" "$DIR/style.css" 5
smoke::assert_alive || fail=1
smoke::assert_clean || fail=1
kill_waybar
echo "::endgroup::"

[ "$fail" = 0 ] || { echo "::error::hardware tier failed"; exit 1; }
echo "✓ fake-hardware tier passed"
