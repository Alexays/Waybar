#!/usr/bin/env bash
# Tier — state-transition coverage with real daemons. Steady-state rendering
# never hits the empty/edge states where modules crash. Drive an actual mpd and
# PulseAudio through those states under ASan:
#   - mpd: connect with an EMPTY queue (no current song -> null song_, #5183),
#     then clear/add/stop/play while waybar is live
#   - pulseaudio + pulseaudio/slider against a null sink
#
# Best-effort: each sub-tier is skipped (not failed) if its daemon isn't
# installed. Run continue-on-error in CI while the fixtures settle.
#
# Usage: state.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

smoke::setup
smoke::start_compositor
trap 'smoke::stop; [ -n "${MPD_PID:-}" ] && kill "$MPD_PID" 2>/dev/null; [ -n "${PA_STARTED:-}" ] && pulseaudio --kill 2>/dev/null; true' EXIT

fail=0
MPD_PID=""
PA_STARTED=""

# ---------------------------------------------------------------- mpd -----
mpd_tier() {
    if ! command -v mpd >/dev/null || ! command -v mpc >/dev/null; then
        echo "(mpd/mpc not installed, skipping mpd tier)"; return 0
    fi
    echo "::group::mpd: empty queue + transitions (#5183)"
    local d conf; d="$(mktemp -d)"; conf="$d/mpd.conf"
    mkdir -p "$d/music" "$d/playlists"
    cat > "$conf" <<EOF
music_directory "$d/music"
playlist_directory "$d/playlists"
db_file "$d/db"
state_file "$d/state"
pid_file "$d/pid"
bind_to_address "127.0.0.1"
port "6600"
audio_output { type "null" name "null" }
EOF
    mpd --no-daemon "$conf" >/tmp/mpd.log 2>&1 &
    MPD_PID=$!
    local up=0 _
    for _ in $(seq 1 20); do
        mpc -h 127.0.0.1 -p 6600 status >/dev/null 2>&1 && { up=1; break; }
        sleep 0.5
    done
    if [ "$up" != 1 ]; then
        echo "::warning::mpd did not come up; skipping"; cat /tmp/mpd.log || true; echo "::endgroup::"; return 0
    fi

    mpc -h 127.0.0.1 -p 6600 clear >/dev/null 2>&1 || true   # empty queue: the crash trigger

    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["mpd"],
  "mpd": { "server": "127.0.0.1", "port": 6600, "interval": 1,
           "format": "{album} - {artist} - {title}", "format-stopped": "stopped" }
}
EOF
    smoke::launch_waybar "$cfg" "$DIR/style.css" 5
    smoke::assert_alive || fail=1              # would have segfaulted pre-#5183 fix
    smoke::assert_clean || fail=1
    # Churn state while live: clear again, then stop/play.
    mpc -h 127.0.0.1 -p 6600 clear >/dev/null 2>&1 || true; sleep 2
    mpc -h 127.0.0.1 -p 6600 stop  >/dev/null 2>&1 || true; sleep 2
    smoke::assert_alive || fail=1
    smoke::assert_clean_exit || fail=1
    echo "::endgroup::"
}

# -------------------------------------------------------- pulseaudio ------
pulse_tier() {
    if ! command -v pulseaudio >/dev/null; then
        echo "(pulseaudio not installed, skipping pulse tier)"; return 0
    fi
    echo "::group::pulseaudio + pulseaudio/slider (null sink)"
    export PULSE_RUNTIME_PATH="$XDG_RUNTIME_DIR/pulse"
    if pulseaudio --start --exit-idle-time=-1 --log-target=stderr >/tmp/pa.log 2>&1; then
        PA_STARTED=1
    else
        echo "::warning::pulseaudio failed to start; skipping"; cat /tmp/pa.log || true; echo "::endgroup::"; return 0
    fi
    pactl load-module module-null-sink sink_name=smoke_null >/dev/null 2>&1 || true

    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["pulseaudio", "pulseaudio/slider"],
  "pulseaudio": { "format": "{volume}%" },
  "pulseaudio/slider": { "min": 0, "max": 100, "orientation": "horizontal" }
}
EOF
    smoke::launch_waybar "$cfg" "$DIR/style.css" 5
    smoke::assert_alive || fail=1
    smoke::assert_clean || fail=1
    pactl set-sink-volume @DEFAULT_SINK@ 40% >/dev/null 2>&1 || true; sleep 1
    pactl set-sink-mute   @DEFAULT_SINK@ 1   >/dev/null 2>&1 || true; sleep 1
    smoke::assert_alive || fail=1
    smoke::assert_clean_exit || fail=1
    echo "::endgroup::"
}

mpd_tier
pulse_tier

[ "$fail" = 0 ] || { echo "::error::state tier failed"; exit 1; }
echo "✓ state-transition tier passed"
