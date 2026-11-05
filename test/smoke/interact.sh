#!/usr/bin/env bash
# Tier 2 — interaction: inject real pointer input through a virtual pointer and
# assert on the behaviour (on-click side effect, format-alt toggle, click-to-reveal
# drawer), not on pixels alone.
#
# Usage: interact.sh [screenshot-dir]
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

SHOTDIR="${1:-interaction-shots}"
mkdir -p "$SHOTDIR"

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

# The headless backend has no input device: a virtual pointer gives the seat one.
CLICK="$(mktemp -d)"
wayland-scanner client-header "$DIR/wlr-virtual-pointer-unstable-v1.xml" \
    "$CLICK/wlr-virtual-pointer-unstable-v1-client-protocol.h"
wayland-scanner private-code "$DIR/wlr-virtual-pointer-unstable-v1.xml" "$CLICK/protocol.c"
# shellcheck disable=SC2046
gcc -I"$CLICK" "$DIR/pointer_click.c" "$CLICK/protocol.c" -o "$CLICK/pointer_click" \
    $(pkg-config --cflags --libs wayland-client)

CX=$((WIDTH / 2)); CY=15

# click_center [evdev button code, BTN_LEFT by default]
click_center() {
    "$CLICK/pointer_click" "$CX" "$CY" "$WIDTH" "$HEIGHT" ${1:+"$1"}
}

# pixel_diff <a.png> <b.png>: number of differing pixels
pixel_diff() {
    local ae
    ae="$(compare -metric AE "$1" "$2" null: 2>&1 || true)"
    printf '%s' "$ae" | grep -oE '^[0-9]+' || echo 0
}

fail=0

# --- 1) on-click side effect ---------------------------------------------
echo "::group::on-click side effect"
SENTINEL="$(mktemp -u)"
cfg1="$(mktemp --suffix=.json)"
cat > "$cfg1" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom/btn"],
  "custom/btn": { "exec": "echo CLICK ME", "interval": "once", "on-click": "touch $SENTINEL" }
}
EOF
smoke::launch_waybar "$cfg1" "$DIR/style.css" 4
smoke::assert_alive || fail=1
smoke::assert_clean || fail=1
click_center
sleep 2
if [ -f "$SENTINEL" ]; then
    echo "✓ on-click ran (sentinel created)"
else
    echo "::error::on-click did not fire — sentinel missing"; fail=1
fi
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 2) format-alt toggle on click ---------------------------------------
echo "::group::format-alt toggle"
cfg2="$(mktemp --suffix=.json)"
cat > "$cfg2" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom/alt"],
  "custom/alt": { "exec": "echo VALUE", "interval": "once", "format": "{}", "format-alt": "ALTERNATE FORMAT SHOWING {}" }
}
EOF
smoke::launch_waybar "$cfg2" "$DIR/style.css" 4
smoke::assert_alive || fail=1
smoke::screenshot "$SHOTDIR/alt-before.png"
click_center
sleep 2
smoke::screenshot "$SHOTDIR/alt-after.png"
ae="$(pixel_diff "$SHOTDIR/alt-before.png" "$SHOTDIR/alt-after.png")"
echo "pixels changed after click: $ae"
if [ "$ae" -gt 100 ]; then
    echo "✓ format-alt toggled the label on click"
else
    echo "::error::format-alt did not change the bar on click ($ae px)"; fail=1
fi
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 3) click-to-reveal drawer with a clickable leader -------------------
# The leader's own on-click-right must not swallow the left click the group needs.
echo "::group::click-to-reveal drawer"
SENTINEL3="$(mktemp -u)"
cfg3="$(mktemp --suffix=.json)"
cat > "$cfg3" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["group/drawer"],
  "group/drawer": {
    "orientation": "horizontal",
    "drawer": { "click-to-reveal": true, "transition-duration": 0 },
    "modules": ["custom/leader", "custom/child"]
  },
  "custom/leader": { "exec": "echo LEADER", "interval": "once", "on-click-right": "touch $SENTINEL3" },
  "custom/child": { "exec": "echo HIDDEN-CHILD-MODULE", "interval": "once" }
}
EOF
smoke::launch_waybar "$cfg3" "$DIR/style.css" 4
smoke::assert_alive || fail=1
smoke::screenshot "$SHOTDIR/drawer-closed.png"
click_center 0x111  # BTN_RIGHT
sleep 2
if [ -f "$SENTINEL3" ]; then
    echo "✓ right click on the leader ran its on-click-right"
else
    echo "::error::right click on the leader did not run its on-click-right"; fail=1
fi
smoke::screenshot "$SHOTDIR/drawer-right-click.png"
ae="$(pixel_diff "$SHOTDIR/drawer-closed.png" "$SHOTDIR/drawer-right-click.png")"
if [ "$ae" -gt 100 ]; then
    echo "::error::right click on the leader changed the bar ($ae px): drawer toggled"; fail=1
fi
click_center
sleep 1
smoke::screenshot "$SHOTDIR/drawer-open.png"
ae="$(pixel_diff "$SHOTDIR/drawer-closed.png" "$SHOTDIR/drawer-open.png")"
echo "pixels changed after left click: $ae"
if [ "$ae" -gt 100 ]; then
    echo "✓ left click on the leader revealed the drawer"
else
    echo "::error::left click on the leader did not reveal the drawer ($ae px)"; fail=1
fi
smoke::assert_clean || fail=1
kill "$WAYBAR_PID" 2>/dev/null || true
echo "::endgroup::"

[ "$fail" = 0 ] || exit 1
echo "✓ interaction tests passed"
