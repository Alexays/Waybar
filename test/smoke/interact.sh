#!/usr/bin/env bash
# Tier 2 — interaction: inject real pointer input via sway and assert on the
# behaviour (on-click side effect, format-alt toggle), not on pixels alone.
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

SEAT="$(swaymsg -t get_seats 2>/dev/null | jq -r '.[0].name' 2>/dev/null || echo seat0)"
CX=$((WIDTH / 2)); CY=15

click_center() {
    swaymsg "seat $SEAT cursor set $CX $CY" >/dev/null
    swaymsg "seat $SEAT cursor press button1" >/dev/null
    swaymsg "seat $SEAT cursor release button1" >/dev/null
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
  "custom/alt": { "exec": "echo VALUE", "interval": "once", "format": "{}", "format-alt": "*** {} ***" }
}
EOF
smoke::launch_waybar "$cfg2" "$DIR/style.css" 4
smoke::assert_alive || fail=1
smoke::screenshot "$SHOTDIR/alt-before.png"
click_center
sleep 2
smoke::screenshot "$SHOTDIR/alt-after.png"
ae="$(compare -metric AE "$SHOTDIR/alt-before.png" "$SHOTDIR/alt-after.png" null: 2>&1 || true)"
ae="$(printf '%s' "$ae" | grep -oE '^[0-9]+' || echo 0)"
echo "pixels changed after click: $ae"
if [ "$ae" -gt 100 ]; then
    echo "✓ format-alt toggled the label on click"
else
    echo "::error::format-alt did not change the bar on click ($ae px)"; fail=1
fi
kill "$WAYBAR_PID" 2>/dev/null || true
echo "::endgroup::"

[ "$fail" = 0 ] || exit 1
echo "✓ interaction tests passed"
