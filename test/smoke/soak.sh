#!/usr/bin/env bash
# Endurance: one bar of fast modules kept running for minutes under reload,
# toggle, realtime-signal and output-hotplug churn. Samples the process's RSS,
# open descriptors, threads and child processes once per cycle and fails when
# the last minute sits above the first one by more than the slack below.
#
# Usage: soak.sh
# Knobs: SOAK_CYCLES (10 s each, default 24), SOAK_WARMUP (seconds, default 30),
#        SOAK_RSS_SLACK_KB (default 8192). Below 12 cycles the growth check is
#        skipped: it needs two disjoint minutes.
set -uo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

CYCLES="${SOAK_CYCLES:-24}"
WARMUP="${SOAK_WARMUP:-30}"
# After warm-up the RSS floor never rises by 1 MB over four minutes; a leak of
# 1 MB per reload clears 8 MB over the 12 or more reloads between the windows.
RSS_SLACK_KB="${SOAK_RSS_SLACK_KB:-8192}"
COUNT_SLACK=3

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

runs="$(mktemp)"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["group/drawer", "custom/fast", "custom/stream"],
  "modules-center": ["clock", "custom/signal"],
  "modules-right": ["cpu", "memory", "custom/json"],
  "group/drawer": {
    "orientation": "inherit",
    "drawer": { "transition-duration": 50 },
    "modules": ["custom/leader", "cpu", "memory", "clock"]
  },
  "custom/leader": { "format": "drawer" },
  "clock": { "interval": 1, "format": "{:%H:%M:%S}" },
  "cpu": { "interval": 1 },
  "memory": { "interval": 0.5 },
  "custom/fast": { "exec": "date +%N", "interval": 0.1 },
  "custom/stream": { "exec": "while :; do date +%N; sleep 0.1; done" },
  "custom/signal": { "exec": "echo x >> $runs; wc -l < $runs", "interval": 0.1, "signal": 1 },
  "custom/json": {
    "exec": "s=\$(date +%-S); printf '{\"text\":\"%s\",\"class\":\"c%s\",\"percentage\":%s}\\\\n' \$(date +%N) \$((s % 4)) \$((s * 100 / 60))",
    "return-type": "json", "interval": 0.1, "signal": 2
  }
}
EOF

fail=0
smoke::launch_waybar "$cfg" "$DIR/style.css" "$WARMUP"
# The first second bar allocates caches that stay for good: part of the warm-up.
smoke::add_output
smoke::remove_output
smoke::assert_alive || exit 1

samples=()
echo "::group::soak: $CYCLES cycles of 10 s"
start=$SECONDS
for c in $(seq 1 "$CYCLES"); do
    smoke::signal SIGUSR2 1                     # reload: every bar and module rebuilt
    for _ in 1 2; do kill -s RTMIN+1 "$WAYBAR_PID"; kill -s RTMIN+2 "$WAYBAR_PID"; sleep 0.5; done
    smoke::signal SIGUSR1 1                     # hide
    smoke::signal SIGUSR1 1                     # show
    if [ $((c % 3)) = 0 ]; then
        smoke::add_output                       # a second Bar for 2 s
        smoke::remove_output
        sleep 2
    else
        for _ in $(seq 1 12); do kill -s RTMIN+1 "$WAYBAR_PID"; sleep 0.5; done
    fi
    smoke::assert_alive || { fail=1; break; }
    rss="$(awk '/^VmRSS:/ {print $2}' "/proc/$WAYBAR_PID/status")"
    threads="$(awk '/^Threads:/ {print $2}' "/proc/$WAYBAR_PID/status")"
    fds="$(find "/proc/$WAYBAR_PID/fd" -mindepth 1 | wc -l)"
    kids="$(pgrep -c -P "$WAYBAR_PID" || true)"
    samples+=("$rss $fds $threads $kids")
    echo "cycle $c t=$((SECONDS - start))s rss_kb=$rss fds=$fds threads=$threads children=$kids"
done
echo "::endgroup::"

# window_min <first cycle index> <cycles> <column>: the floor of that window.
# Transient peaks during a reload do not count, a leak raises the floor.
window_min() {
    printf '%s\n' "${samples[@]:$1:$2}" | awk -v c="$3" 'NR == 1 || $c < m {m = $c} END {print m}'
}

if [ "$fail" = 0 ] && [ "$CYCLES" -ge 12 ]; then
    last=$((CYCLES - 6))
    name=(_ rss_kb fds threads children)
    for col in 1 2 3 4; do
        slack=$COUNT_SLACK; [ "$col" = 1 ] && slack=$RSS_SLACK_KB
        first_min="$(window_min 0 6 "$col")"
        last_min="$(window_min "$last" 6 "$col")"
        if [ $((last_min - first_min)) -gt "$slack" ]; then
            echo "::error::${name[$col]} grew from $first_min (first minute) to $last_min (last minute), slack $slack"
            fail=1
        else
            echo "✓ ${name[$col]} stable: $first_min (first minute) vs $last_min (last minute), slack $slack"
        fi
    done
fi

# The signal module still runs its command after all the churn.
before="$(wc -l < "$runs")"
kill -s RTMIN+1 "$WAYBAR_PID" 2>/dev/null
sleep 1
if [ "$(wc -l < "$runs")" -le "$before" ]; then
    echo "::error::custom/signal no longer runs its command"; fail=1
else
    echo "✓ custom/signal still runs its command"
fi

smoke::assert_alive || fail=1
smoke::assert_clean || fail=1
smoke::assert_clean_exit || fail=1

[ "$fail" = 0 ] || { echo "::error::soak tier failed"; exit 1; }
echo "✓ soak tier passed"
