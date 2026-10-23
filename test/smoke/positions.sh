#!/usr/bin/env bash
# Tier 3 — layout matrix: bar at top/bottom (horizontal) and left (vertical),
# plus a HiDPI (scale 2) run. Each must launch and render without a crash or
# sanitizer/critical error. Screenshots are uploaded for visual inspection.
#
# Usage: positions.sh [screenshot-dir]
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

SHOTDIR="${1:-layout-shots}"
mkdir -p "$SHOTDIR"

smoke::setup
trap smoke::stop EXIT

fails=()

# run_case <name> <position> <scale>
run_case() {
    local name="$1" pos="$2" scale="$3"
    echo "::group::layout: $name (position=$pos scale=$scale)"
    SCALE="$scale" smoke::start_compositor || { fails+=("$name"); echo "::endgroup::"; return; }
    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<EOF
{
  "layer": "top", "position": "$pos", "height": 30,
  "modules-left": ["custom/l"], "modules-center": ["custom/c"], "modules-right": ["custom/r"],
  "custom/l": { "exec": "echo LEFT",   "interval": "once" },
  "custom/c": { "exec": "echo CENTER", "interval": "once" },
  "custom/r": { "exec": "echo RIGHT",  "interval": "once" }
}
EOF
    smoke::launch_waybar "$cfg" "$DIR/style.css" 4
    local ok=1
    smoke::assert_alive || ok=0
    smoke::assert_clean || ok=0
    smoke::screenshot "$SHOTDIR/$name.png" || true
    [ "$ok" = 1 ] || fails+=("$name")
    smoke::stop
    echo "::endgroup::"
}

run_case top-x1    top    1
run_case bottom-x1 bottom 1
run_case left-x1   left   1
run_case top-x2    top    2

if [ "${#fails[@]}" -gt 0 ]; then
    echo "::error::layout cases failed: ${fails[*]}"
    exit 1
fi
echo "✓ all layout cases launched and rendered cleanly"
