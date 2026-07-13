#!/usr/bin/env bash
# Report-only leak check. Runs the deterministic config, exits cleanly (SIGINT,
# so LeakSanitizer's atexit hook actually runs -- a SIGTERM kill would skip it),
# and surfaces any leak report. Never fails the job: leaks in GTK/GLib are too
# noisy to gate on, but regressions in our own code are worth seeing.
#
# Usage: leakcheck.sh
set -uo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

export SMOKE_DETECT_LEAKS=1
smoke::setup
# Report leaks without aborting: abort_on_error=1 (the default gate) would turn
# LSan's exit-time report into a core dump. This tier only reports.
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:abort_on_error=0:detect_odr_violation=0"
smoke::start_compositor
trap smoke::stop EXIT

smoke::launch_waybar "$DIR/config.jsonc" "$DIR/style.css" 5
smoke::assert_alive || true

# Clean exit so LSan runs.
kill -INT "$WAYBAR_PID" 2>/dev/null || true
for _ in $(seq 1 20); do kill -0 "$WAYBAR_PID" 2>/dev/null || break; sleep 0.5; done
kill -9 "$WAYBAR_PID" 2>/dev/null || true

if grep -iqE 'LeakSanitizer|Direct leak|Indirect leak' "$SMOKE_LOG"; then
    echo "::warning::LeakSanitizer reported leaks (report-only)"
    echo "::group::leak report"
    grep -iE 'leak|SUMMARY|#[0-9]+ 0x' "$SMOKE_LOG" || true
    echo "::endgroup::"
else
    echo "✓ no leaks reported"
fi
WAYBAR_PID=""
