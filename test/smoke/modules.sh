#!/usr/bin/env bash
# Tier 1 — per-module matrix: render each headless-safe module in isolation and
# assert it loads, logs no sanitizer/critical error, and draws something.
#
# Usage: modules.sh <screenshot-dir>
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

SHOTDIR="${1:-module-shots}"
mkdir -p "$SHOTDIR"

# module name -> its config options (JSON object). Only modules that work with
# no real hardware/daemon in a headless runner are included.
declare -A MODULES=(
    [clock]='{}'
    [cpu]='{}'
    [memory]='{}'
    [disk]='{}'
    [idle_inhibitor]='{"format":"{status}"}'
    [custom/echo]='{"exec":"echo hello","interval":"once"}'
)

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

fails=()
for mod in "${!MODULES[@]}"; do
    safe="${mod//\//-}"
    cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["$mod"],
  "$mod": ${MODULES[$mod]}
}
EOF
    echo "::group::module: $mod"
    smoke::launch_waybar "$cfg" "$DIR/style.css" 4
    ok=1
    smoke::assert_alive || ok=0
    smoke::assert_clean || ok=0
    if [ "$ok" = 1 ]; then
        smoke::screenshot "$SHOTDIR/$safe.png"
        smoke::assert_not_blank "$SHOTDIR/$safe.png" || ok=0
    fi
    [ "$ok" = 1 ] || fails+=("$mod")
    kill "$WAYBAR_PID" 2>/dev/null || true
    sleep 1
    echo "::endgroup::"
done

if [ "${#fails[@]}" -gt 0 ]; then
    echo "::error::modules failed to render cleanly: ${fails[*]}"
    exit 1
fi
echo "✓ all ${#MODULES[@]} modules rendered cleanly"
