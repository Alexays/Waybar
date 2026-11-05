#!/usr/bin/env bash
# Tier 2 — interaction: inject real pointer input through a virtual pointer and
# assert on the behaviour (click, scroll and button commands, format-alt toggle,
# click-to-reveal drawer, menu popup, tooltips), not on pixels alone.
# Needs a D-Bus session for the AT-SPI checks:
#   dbus-run-session -- test/smoke/interact.sh [screenshot-dir]
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

SHOTDIR="${1:-interaction-shots}"
mkdir -p "$SHOTDIR"

export SMOKE_A11Y=1
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

# pointer <x> [action] [hold_ms]: pointer input on the bar row, see pointer_click.c
pointer() {
    "$CLICK/pointer_click" "$1" "$CY" "$WIDTH" "$HEIGHT" "${@:2}"
}

# expect_log <file> <expected words> <what>: the commands appended these lines, in order
expect_log() {
    local got=""
    for _ in $(seq 1 12); do
        got="$(tr '\n' ' ' < "$1" 2>/dev/null || true)"
        [ "$got" = "${2:+$2 }" ] && { echo "✓ $3"; return 0; }
        sleep 0.25
    done
    echo "::error::$3: expected '$2', got '$got'"; return 1
}

# tooltip_shown <x>: anything drawn on the black output just below the bar, from x on
tooltip_shown() {
    local shot mean
    shot="$(mktemp --suffix=.png)"
    grim -g "$1,32 400x60" "$shot"
    mean="$(convert "$shot" -format '%[fx:mean]' info:)"
    awk "BEGIN{ exit !($mean > 0.01) }"
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
pointer "$CX"
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
pointer "$CX"
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
pointer "$CX" 0x111  # BTN_RIGHT
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
pointer "$CX"
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
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 4) scroll directions and smooth-scrolling-threshold -----------------
# A wheel notch reaches GTK as a smooth delta of 1.5, so a threshold of 2 needs two notches.
echo "::group::scroll"
SCROLL_LOG="$(mktemp)"; SMOOTH_LOG="$(mktemp)"
cfg4="$(mktemp --suffix=.json)"
cat > "$cfg4" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["custom/scroll"], "modules-right": ["custom/smooth"],
  "custom/scroll": {
    "exec": "echo SCROLL-TARGET-MODULE", "interval": "once",
    "on-scroll-up": "echo up >> $SCROLL_LOG", "on-scroll-down": "echo down >> $SCROLL_LOG",
    "on-scroll-left": "echo left >> $SCROLL_LOG", "on-scroll-right": "echo right >> $SCROLL_LOG"
  },
  "custom/smooth": {
    "exec": "echo SMOOTH-TARGET-MODULE", "interval": "once",
    "smooth-scrolling-threshold": 2, "on-scroll-down": "echo down >> $SMOOTH_LOG"
  }
}
EOF
smoke::launch_waybar "$cfg4" "$DIR/style.css" 4
smoke::assert_alive || fail=1
for dir in up down left right; do pointer 40 "$dir"; done
expect_log "$SCROLL_LOG" "up down left right" "on-scroll-up/down/left/right ran in order" || fail=1
pointer $((WIDTH - 40)) down
sleep 1
expect_log "$SMOOTH_LOG" "" "one notch stays under smooth-scrolling-threshold" || fail=1
pointer $((WIDTH - 40)) down
expect_log "$SMOOTH_LOG" "down" "two notches cross smooth-scrolling-threshold once" || fail=1
smoke::assert_clean || fail=1
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 5) middle, back, forward and double click ---------------------------
echo "::group::buttons"
BUTTON_LOG="$(mktemp)"
cfg5="$(mktemp --suffix=.json)"
cat > "$cfg5" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom/buttons"],
  "custom/buttons": {
    "exec": "echo BUTTONS-TARGET-MODULE", "interval": "once",
    "on-click": "echo click >> $BUTTON_LOG", "on-double-click": "echo double >> $BUTTON_LOG",
    "on-click-middle": "echo middle >> $BUTTON_LOG",
    "on-click-backward": "echo backward >> $BUTTON_LOG", "on-click-forward": "echo forward >> $BUTTON_LOG"
  }
}
EOF
smoke::launch_waybar "$cfg5" "$DIR/style.css" 4
smoke::assert_alive || fail=1
# BTN_MIDDLE, BTN_SIDE and BTN_EXTRA are GTK buttons 2, 8 and 9
for action in 0x110 0x112 0x113 0x114 double; do pointer "$CX" "$action"; done
# GTK reports both presses of a double click before the double click itself
expect_log "$BUTTON_LOG" "click middle backward forward click click double" \
    "on-click, on-click-middle/backward/forward and on-double-click ran" || fail=1
smoke::assert_clean || fail=1
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 6) menu popup and its action ----------------------------------------
echo "::group::menu"
MENU_LOG="$(mktemp)"
menu_ui="$(mktemp --suffix=.ui)"
cat > "$menu_ui" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <object class="GtkMenu" id="menu">
    <child><object class="GtkMenuItem" id="first"><property name="label">SMOKE MENU FIRST</property></object></child>
    <child><object class="GtkMenuItem" id="second"><property name="label">SMOKE MENU SECOND</property></object></child>
  </object>
</interface>
EOF
cfg6="$(mktemp --suffix=.json)"
cat > "$cfg6" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom/menu"],
  "custom/menu": {
    "exec": "echo MENU-TARGET-MODULE", "interval": "once",
    "menu": "on-click-right", "menu-file": "$menu_ui",
    "menu-actions": { "first": "echo first >> $MENU_LOG", "second": "echo second >> $MENU_LOG" }
  }
}
EOF
smoke::launch_waybar "$cfg6" "$DIR/style.css" 4
smoke::assert_alive || fail=1
python3 "$DIR/a11y.py" --absent "SMOKE MENU FIRST" || fail=1
# This device stays while the item is clicked: losing the seat's pointer would close the menu.
pointer "$CX" 0x111 4000 &
holder=$!
python3 "$DIR/a11y.py" "SMOKE MENU FIRST" "SMOKE MENU SECOND" || fail=1
smoke::screenshot "$SHOTDIR/menu-open.png"
# The menu opens at the pointer with items about 25px tall: the second one is around y=57
"$CLICK/pointer_click" $((CX + 40)) 57 "$WIDTH" "$HEIGHT"
wait "$holder"
expect_log "$MENU_LOG" "second" "the clicked menu item ran its menu-actions command" || fail=1
smoke::assert_clean || fail=1
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 7) tooltips on hover, and kept while the module updates -------------
# GTK does not expose tooltip windows over AT-SPI: look at the black output under the bar instead.
echo "::group::tooltips"
cfg7="$(mktemp --suffix=.json)"
cat > "$cfg7" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["custom/tip"], "modules-right": ["custom/ticking"],
  "custom/tip": { "exec": "echo TOOLTIP-TARGET-MODULE", "interval": "once", "tooltip-format": "STATIC TOOLTIP" },
  "custom/ticking": { "exec": "date +%s.%N", "interval": 1 }
}
EOF
smoke::launch_waybar "$cfg7" "$DIR/style.css" 4
smoke::assert_alive || fail=1
if tooltip_shown 0; then
    echo "::error::something is drawn under the bar before any hover"; fail=1
fi
pointer 40 hover 2500 &
holder=$!
sleep 1.5
if tooltip_shown 0; then
    echo "✓ hovering shows the tooltip"
else
    echo "::error::hovering did not show the tooltip"; fail=1
fi
smoke::screenshot "$SHOTDIR/tooltip.png"
wait "$holder"
sleep 0.5
if tooltip_shown 0; then
    echo "::error::the tooltip stayed after the pointer left"; fail=1
fi
pointer $((WIDTH - 40)) hover 5000 &
holder=$!
sleep 1.5
hidden=0
for _ in 1 2 3 4 5 6; do
    tooltip_shown $((WIDTH - 400)) || hidden=$((hidden + 1))
    sleep 0.5
done
wait "$holder"
if [ "$hidden" = 0 ]; then
    echo "✓ the tooltip of a module updating every second stays shown"
else
    echo "::error::the tooltip of a module updating every second was hidden in $hidden of 6 samples"; fail=1
fi
smoke::assert_clean || fail=1
kill "$WAYBAR_PID" 2>/dev/null || true
echo "::endgroup::"

[ "$fail" = 0 ] || exit 1
echo "✓ interaction tests passed"
