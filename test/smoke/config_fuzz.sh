#!/usr/bin/env bash
# Tier: config robustness. Every documented option of every module that starts
# headless is set to wrong JSON types and edge values (config_fuzz.py), many
# module instances per bar. Each bar must start, keep running, log no sanitizer
# report or critical, and exit cleanly. A failing bar is re-run one option at a
# time so the error names the option.
#
# Usage: config_fuzz.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

OUT="$(mktemp -d)"
python3 "$DIR/config_fuzz.py" "$OUT"

# run_bar <config>: 0 when the bar builds its widgets, survives 2 s of updates
# and exits cleanly; otherwise prints the relevant part of the log.
run_bar() {
    local ok=0 _
    smoke::launch_waybar "$1" "$DIR/style.css" 0
    for _ in $(seq 1 300); do
        grep -q 'GTK widget tree' "$SMOKE_LOG" && break
        kill -0 "$WAYBAR_PID" 2>/dev/null || break
        sleep 0.2
    done
    sleep 2
    smoke::assert_alive >/dev/null || { echo "waybar exited"; ok=1; }
    smoke::assert_clean_exit >/dev/null || ok=1
    if [ "$ok" = 1 ] || grep -q '\[critical\]' "$SMOKE_LOG"; then
        grep -m1 -A40 -E '\[critical\]|ERROR: AddressSanitizer|runtime error:|CRITICAL|terminate called|Sanitizer' "$SMOKE_LOG" || smoke::log | tail -20
        return 1
    fi
}

fail=0
while IFS=$'\t' read -r -u 3 cfg count opts; do
    echo "::group::options: $opts"
    if run_bar "$cfg"; then
        echo "✓ $count instances clean (${SECONDS}s)"
        echo "::endgroup::"
        continue
    fi
    echo "::endgroup::"
    for opt in $opts; do
        one="$(mktemp -d)"
        python3 "$DIR/config_fuzz.py" "$one" --only "$opt"
        while IFS=$'\t' read -r c _ _; do
            run_bar "$c" || { echo "::error::option '$opt' breaks the bar"; fail=1; }
        done < "$one/batches.tsv"
    done
done 3< "$OUT/batches.tsv"

[ "$fail" = 0 ] || { echo "::error::config fuzz tier failed"; exit 1; }
echo "✓ config fuzz passed"
