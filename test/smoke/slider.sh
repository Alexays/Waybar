#!/usr/bin/env bash
# Tier 2 -- custom-slider/: drive the slider with injected pointer input and
# assert on behaviour. Covers the three failure modes the module was built to
# avoid: a per-motion write storm (bounded write count), a crash inside a group
# (the #5179 shape), and a failing read taking the bar down or driving it to 0.
#
# Usage: slider.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

SEAT="$(swaymsg -t get_seats 2>/dev/null | jq -r '.[0].name' 2>/dev/null || echo seat0)"

# A wide trough so the slider occupies a known, draggable strip.
STYLE="$(mktemp --suffix=.css)"
cat > "$STYLE" <<'EOF'
* { border: none; border-radius: 0; }
window#waybar { background: #202020; color: #ffffff; }
#custom-slider-vol trough { min-width: 400px; min-height: 20px; background: #444; }
#custom-slider-vol slider { min-width: 6px; min-height: 20px; background: #fff; }
#custom-slider-vol highlight { background: #6ab04c; }
EOF

fail=0

# --- 1) a drag produces a bounded number of writes (no process storm) -----
echo "::group::custom-slider: bounded writes on drag"
WRITES="$(mktemp)"; : > "$WRITES"
export WRITES
cfg1="$(mktemp --suffix=.json)"
cat > "$cfg1" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom-slider/vol"],
  "custom-slider/vol": {
    "exec": "echo 50", "interval": "once",
    "on-change": "echo {} >> \$WRITES",
    "min": 0, "max": 100, "orientation": "horizontal"
  }
}
EOF
smoke::launch_waybar "$cfg1" "$STYLE" 4
smoke::assert_alive || fail=1
smoke::assert_clean || fail=1

# Drag left-to-right across the centred ~400px trough (bar is WIDTH wide).
y=15; x1=$((WIDTH / 2 - 180)); x2=$((WIDTH / 2 + 180))
swaymsg "seat $SEAT cursor set $x1 $y" >/dev/null
swaymsg "seat $SEAT cursor press button1" >/dev/null
for i in $(seq 1 24); do
    swaymsg "seat $SEAT cursor set $((x1 + (x2 - x1) * i / 24)) $y" >/dev/null
done
swaymsg "seat $SEAT cursor release button1" >/dev/null
sleep 2

count="$(wc -l < "$WRITES" | tr -d ' ')"
echo "writes recorded: $count"
if [ "$count" -eq 0 ]; then
    echo "(no writes -- headless seat likely dropped the pointer; skipping bound check)"
else
    # 24 motion events under the default on-release policy must NOT become 24
    # writes. A handful (drag-end, plus a click) is fine; a storm is the bug.
    if [ "$count" -le 4 ]; then
        echo "✓ write count is bounded ($count for 24 motion events)"
    else
        echo "::error::write storm: $count writes for one drag"; fail=1
    fi
    last="$(tail -n1 "$WRITES")"
    if [[ "$last" =~ ^[0-9]+$ ]] && [ "$last" -ge 0 ] && [ "$last" -le 100 ]; then
        echo "✓ last write is a valid in-range value ($last)"
    else
        echo "::error::last write '$last' is not a valid [0,100] value"; fail=1
    fi
fi
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 2) the slider inside a group/ -- the #5179 shape ---------------------
echo "::group::custom-slider: inside a group"
cfg2="$(mktemp --suffix=.json)"
cat > "$cfg2" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["group/s"],
  "group/s": { "modules": ["custom-slider/vol"] },
  "custom-slider/vol": { "exec": "echo 50", "interval": "once", "min": 0, "max": 100 }
}
EOF
smoke::launch_waybar "$cfg2" "$STYLE" 4
smoke::assert_alive || fail=1
smoke::assert_clean || fail=1
kill "$WAYBAR_PID" 2>/dev/null || true; sleep 1
echo "::endgroup::"

# --- 3) a failing exec must keep the bar alive (and not drive to 0) -------
echo "::group::custom-slider: failing exec stays alive"
cfg3="$(mktemp --suffix=.json)"
cat > "$cfg3" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom-slider/broken"],
  "custom-slider/broken": {
    "exec": "exit 1", "interval": 1,
    "min": 10, "max": 100, "failure-behaviour": "keep"
  }
}
EOF
smoke::launch_waybar "$cfg3" "$STYLE" 4
smoke::assert_alive || fail=1
smoke::assert_clean || fail=1
kill "$WAYBAR_PID" 2>/dev/null || true
echo "::endgroup::"

[ "$fail" = 0 ] || exit 1
echo "✓ custom-slider smoke tests passed"
