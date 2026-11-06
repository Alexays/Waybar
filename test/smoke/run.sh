#!/usr/bin/env bash
# Launch Waybar inside a headless compositor and verify it starts, stays alive,
# logs no sanitizer/critical error, and (optionally) renders a screenshot.
#
# Usage: run.sh <config> [style] [screenshot.png]
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

CONFIG="$1"; STYLE="${2:-}"; SHOT="${3:-}"

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT
smoke::launch_waybar "$CONFIG" "$STYLE"

# ASan and software GL can delay the first map past the launch settle
if [ -n "$SHOT" ]; then
    for _ in $(seq 1 60); do
        grep -q "Bar configured" "$SMOKE_LOG" && break
        sleep 0.5
    done
fi

echo "::group::waybar log"; smoke::log; echo "::endgroup::"

smoke::assert_alive
smoke::assert_clean
if [ -n "$SHOT" ]; then
    grep -q "Bar configured" "$SMOKE_LOG" || { echo "::error::bar never configured"; exit 1; }
    sleep 1
    smoke::screenshot "$SHOT"
fi
