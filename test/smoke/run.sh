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

echo "::group::waybar log"; smoke::log; echo "::endgroup::"

smoke::assert_alive
smoke::assert_clean
[ -n "$SHOT" ] && smoke::screenshot "$SHOT"
