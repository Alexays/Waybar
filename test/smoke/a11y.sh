#!/usr/bin/env bash
# Tier 2 — accessibility assertions. Must run under a D-Bus session so the
# AT-SPI bus is available, e.g.:  dbus-run-session -- test/smoke/a11y.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

# Make sure GTK registers with the AT-SPI bridge.
unset NO_AT_BRIDGE
export GTK_A11Y=1

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT
smoke::launch_waybar "$DIR/config.jsonc" "$DIR/style.css" 6
smoke::assert_alive
python3 "$DIR/a11y.py"
