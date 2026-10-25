#!/usr/bin/env bash
# Keep PulseAudio shared pools bounded when SIGUSR2 recreates the audio backend.
# RSS misses untouched pools, so inspect the real process's shared mappings.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

if [ "$(uname -s)" != Linux ]; then
    echo "(PulseAudio mapping check requires Linux /proc, skipping)"
    exit 0
fi

for tool in pulseaudio pactl; do
    command -v "$tool" >/dev/null || { echo "::error::$tool is required"; exit 1; }
done

PA_PID=""
smoke::setup
probe_dir="$XDG_RUNTIME_DIR"
# Never let an inherited compositor socket select the user's desktop.
unset SWAYSOCK WAYLAND_DISPLAY

cleanup() {
    smoke::stop
    if [ -n "$PA_PID" ]; then
        kill "$PA_PID" 2>/dev/null || true
        wait "$PA_PID" 2>/dev/null || true
    fi
    rm -rf -- "$probe_dir"
}
trap cleanup EXIT

export PULSE_RUNTIME_PATH="$probe_dir/pulse"
export PULSE_STATE_PATH="$probe_dir/pulse-state"
export PULSE_SERVER="unix:$PULSE_RUNTIME_PATH/native"
export PULSE_CLIENTCONFIG="$probe_dir/client.conf"
mkdir -p "$PULSE_RUNTIME_PATH" "$PULSE_STATE_PATH"
cat > "$PULSE_CLIENTCONFIG" <<'EOF'
autospawn = no
enable-shm = yes
enable-memfd = yes
EOF
cat > "$probe_dir/pulse.conf" <<EOF
load-module module-native-protocol-unix socket=$PULSE_RUNTIME_PATH/native auth-anonymous=1
load-module module-null-sink sink_name=waybar_reload_test
set-default-sink waybar_reload_test
set-default-source waybar_reload_test.monitor
EOF
pulseaudio -n --file="$probe_dir/pulse.conf" --daemonize=no \
    --use-pid-file=no --exit-idle-time=-1 >"$probe_dir/pulse.log" 2>&1 &
PA_PID=$!

ready=0
for _ in $(seq 1 100); do
    if pactl info >/dev/null 2>&1; then ready=1; break; fi
    kill -0 "$PA_PID" 2>/dev/null || break
    sleep 0.1
done
if [ "$ready" != 1 ]; then
    echo "::error::private PulseAudio server did not become ready"
    cat "$probe_dir/pulse.log"
    exit 1
fi

smoke::start_compositor
cat > "$probe_dir/waybar.json" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["pulseaudio"],
  "pulseaudio": { "format": "{volume}%" }
}
EOF
smoke::launch_waybar "$probe_dir/waybar.json" "$DIR/style.css" 1
smoke::assert_alive

pulse_pool_count() {
    awk '/\/memfd:pulseaudio|\/dev\/shm\/pulse-shm-/ { count++ }
         END { print count + 0 }' "/proc/$WAYBAR_PID/maps"
}

baseline=0
for _ in $(seq 1 100); do
    baseline="$(pulse_pool_count)"
    [ "$baseline" -gt 0 ] && break
    smoke::assert_alive >/dev/null
    sleep 0.1
done
if [ "$baseline" -eq 0 ]; then
    echo "::error::no PulseAudio shared pool was mapped; regression was not exercised"
    smoke::log
    exit 1
fi
echo "PulseAudio shared pools before reload: $baseline"

for reload in $(seq 1 8); do
    before="$(grep -c 'Bar configured' "$SMOKE_LOG" || true)"
    kill -USR2 "$WAYBAR_PID"
    configured=0
    for _ in $(seq 1 100); do
        smoke::assert_alive >/dev/null
        after="$(grep -c 'Bar configured' "$SMOKE_LOG" || true)"
        if [ "$after" -gt "$before" ]; then configured=1; break; fi
        sleep 0.1
    done
    if [ "$configured" != 1 ]; then
        echo "::error::reload $reload did not finish"
        smoke::log
        exit 1
    fi
    pools="$(pulse_pool_count)"
    echo "PulseAudio shared pools after reload $reload: $pools"
    if [ "$pools" -ne "$baseline" ]; then
        echo "::error::PulseAudio pool count changed from $baseline to $pools"
        smoke::log
        exit 1
    fi
done

smoke::assert_clean_exit
echo "✓ PulseAudio shared pools remained bounded across 8 reloads"
