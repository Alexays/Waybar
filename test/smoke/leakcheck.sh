#!/usr/bin/env bash
# Leak check. Runs the deterministic config, exits cleanly (SIGINT, so
# LeakSanitizer's atexit hook actually runs -- a SIGTERM kill would skip it),
# and fails on any leak left once the third-party ones in lsan.supp are set
# aside.
#
# Usage: leakcheck.sh
set -uo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

export SMOKE_DETECT_LEAKS=1
smoke::setup
# abort_on_error=1 (the default gate) would turn LSan's exit-time report into a
# core dump; the report itself is what assert_clean_exit looks for.
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:abort_on_error=0:detect_odr_violation=0"
export LSAN_OPTIONS="suppressions=$DIR/lsan.supp"
smoke::start_compositor
trap smoke::stop EXIT

smoke::launch_waybar "$DIR/config.jsonc" "$DIR/style.css" 5
smoke::assert_alive || exit 1
smoke::assert_clean_exit || { echo "::error::leak check failed"; exit 1; }
echo "✓ no leaks outside lsan.supp"
