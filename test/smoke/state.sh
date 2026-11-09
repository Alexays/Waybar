#!/usr/bin/env bash
# Tier 5 - state transitions with real daemons. Steady-state rendering never
# hits the empty/edge states where modules crash. Drive an actual mpd and
# PulseAudio through those states under ASan and assert the text each state
# shows, through the AT-SPI tree:
#   - mpd: an EMPTY queue (no current song, #5183), then play, next track,
#     pause, stop, clear, and the server going away
#   - pulseaudio + pulseaudio/slider on a null sink: volume and mute changes
# plus two stand-in servers (an unresponsive mpd, a pulse server dropping every
# connection).
#
# Usage: dbus-run-session -- state.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

fail=0
MPD_PID=""
PA_STARTED=""
A11Y=(python3 "$DIR/a11y.py" --timeout 8)

cleanup() {
    # Tear our daemons down first: smoke::stop waits on the compositor/waybar,
    # and a still-running mpd/pulseaudio must not be left for anything to block on.
    [ -n "$MPD_PID" ] && kill "$MPD_PID" 2>/dev/null || true
    [ -n "$PA_STARTED" ] && pulseaudio --kill 2>/dev/null || true
    smoke::stop
    return 0
}

export SMOKE_A11Y=1
smoke::setup
smoke::start_compositor
trap cleanup EXIT

# ---------------------------------------------------------------- mpd -----
mpd_tier() {
    echo "::group::mpd: empty queue, track and state transitions (#5183)"
    local d; d="$(mktemp -d)"
    mkdir -p "$d/music" "$d/playlists"
    # Two 30 s silent tracks; the null output plays them in real time.
    python3 - "$d/music" <<'PY'
import sys, wave
for name in ("one.wav", "two.wav"):
    with wave.open(f"{sys.argv[1]}/{name}", "wb") as w:
        w.setnchannels(1); w.setsampwidth(1); w.setframerate(8000)
        w.writeframes(b"\x80" * 8000 * 30)
PY
    # Not 6600: installing mpd on Ubuntu starts a system instance there.
    cat > "$d/mpd.conf" <<EOF
music_directory "$d/music"
playlist_directory "$d/playlists"
db_file "$d/db"
state_file "$d/state"
pid_file "$d/pid"
bind_to_address "127.0.0.1"
port "6612"
audio_output {
    type "null"
    name "null"
}
EOF
    mpd --no-daemon "$d/mpd.conf" >"$d/mpd.log" 2>&1 &
    MPD_PID=$!
    local mpc=(mpc -q -h 127.0.0.1 -p 6612) up=0 _
    for _ in $(seq 1 20); do
        "${mpc[@]}" status >/dev/null 2>&1 && { up=1; break; }
        sleep 0.5
    done
    if [ "$up" != 1 ]; then
        echo "::error::mpd did not come up"; cat "$d/mpd.log"; fail=1; echo "::endgroup::"; return 0
    fi
    "${mpc[@]}" update --wait
    "${mpc[@]}" clear                       # empty queue: the #5183 trigger

    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["mpd"],
  "mpd": { "server": "127.0.0.1", "port": 6612, "interval": 1,
           "format": "{stateIcon} {filename} {songPosition}/{queueLength}",
           "state-icons": { "playing": "PLAY", "paused": "PAUSE" },
           "format-stopped": "STOPPED" }
}
EOF
    smoke::launch_waybar "$cfg" "$DIR/style.css" 1
    "${A11Y[@]}" "STOPPED" || fail=1
    "${mpc[@]}" add one.wav two.wav
    "${mpc[@]}" play
    "${A11Y[@]}" "PLAY one.wav 1/2" || fail=1
    "${mpc[@]}" next
    "${A11Y[@]}" "PLAY two.wav 2/2" || fail=1
    "${mpc[@]}" pause
    "${A11Y[@]}" "PAUSE two.wav 2/2" || fail=1
    "${mpc[@]}" stop
    "${A11Y[@]}" "STOPPED" || fail=1
    "${mpc[@]}" clear
    # Only survival is checked once the server is gone: format-disconnected
    # hides the label instead of showing it.
    kill "$MPD_PID"; wait "$MPD_PID" 2>/dev/null || true; MPD_PID=""
    sleep 2
    smoke::assert_alive || fail=1
    smoke::assert_clean_exit || fail=1
    echo "::endgroup::"
}

# -------------------------------------------------------- pulseaudio ------
pulse_tier() {
    echo "::group::pulseaudio + pulseaudio/slider (null sink)"
    export PULSE_RUNTIME_PATH="$XDG_RUNTIME_DIR/pulse"
    local log; log="$(mktemp)"
    if ! pulseaudio --start --exit-idle-time=-1 --log-target=stderr >"$log" 2>&1; then
        echo "::error::pulseaudio failed to start"; cat "$log"; fail=1; echo "::endgroup::"; return 0
    fi
    PA_STARTED=1
    pactl load-module module-null-sink sink_name=smoke_null >/dev/null
    pactl set-default-sink smoke_null
    pactl set-sink-volume smoke_null 40%

    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["pulseaudio", "pulseaudio/slider"],
  "pulseaudio": { "format": "VOL {volume}%", "format-muted": "MUTED {volume}%" },
  "pulseaudio/slider": { "min": 0, "max": 100, "orientation": "horizontal" }
}
EOF
    smoke::launch_waybar "$cfg" "$DIR/style.css" 1
    "${A11Y[@]}" "VOL 40%" "value=40" || fail=1
    pactl set-sink-volume smoke_null 75%
    "${A11Y[@]}" "VOL 75%" "value=75" || fail=1
    pactl set-sink-mute smoke_null 1
    "${A11Y[@]}" "MUTED 75%" || fail=1
    pactl set-sink-mute smoke_null 0
    "${A11Y[@]}" "VOL 75%" || fail=1
    smoke::assert_clean_exit || fail=1
    echo "::endgroup::"
}

# ------------------------------------------ mpd: unresponsive server -----
# A local process on the mpd port that greets, then never finishes its reply
# (each line restarts libmpdclient's read timeout), must not freeze the bar:
# SIGINT still has to be acted on. A python stand-in is enough, no mpd needed.
mpd_unresponsive_tier() {
    if ! command -v python3 >/dev/null; then
        echo "(python3 not installed, skipping mpd stand-in tier)"; return 0
    fi
    echo "::group::mpd: unresponsive server must not block the bar"
    local d; d="$(mktemp -d)"
    python3 - "$d/status-sent" <<'PY' &
import socket, sys, time
listener = socket.socket()
listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listener.bind(("127.0.0.1", 6611))
listener.listen()
while True:
    bar, _ = listener.accept()
    bar.sendall(b"OK MPD 0.23.5\n")
    bar.recv(100)                         # the bar's "status"
    open(sys.argv[1], "w").close()
    try:
        while True:
            bar.sendall(b"volume: 50\n")  # a line every second, never "OK"
            time.sleep(1)
    except OSError:
        pass                              # the bar went away
PY
    local standin=$!

    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["mpd", "clock"],
  "mpd": { "server": "127.0.0.1", "port": 6611, "interval": 1 },
  "clock": { "interval": 1 }
}
EOF
    smoke::launch_waybar "$cfg" "$DIR/style.css" 1
    local _; for _ in $(seq 1 30); do [ -e "$d/status-sent" ] && break; sleep 0.5; done
    [ -e "$d/status-sent" ] || echo "::warning::the bar never asked the stand-in for its status"
    sleep 3                                   # the reply is still dripping in
    smoke::assert_alive || fail=1
    smoke::assert_clean_exit || fail=1        # hangs if the main loop sits in libmpdclient
    kill "$standin" 2>/dev/null || true; wait "$standin" 2>/dev/null || true
    echo "::endgroup::"
}

mpd_tier
pulse_tier
mpd_unresponsive_tier

# ---------------------------------------------- pulseaudio reconnect ------
# A context that keeps failing after it connected must be retried at a bounded
# pace, not in a busy loop. No daemon needed: a stand-in accepts and drops every
# connection for as long as it runs.
pulse_reconnect_tier() {
    echo "::group::pulseaudio: reconnect after context failure is rate-limited"
    local sock="$XDG_RUNTIME_DIR/pulse-drop.sock" hits standin; hits="$(mktemp)"
    python3 - "$sock" >"$hits" <<'EOF' &
import socket, sys
srv = socket.socket(socket.AF_UNIX)
srv.bind(sys.argv[1])
srv.listen(64)
while True:
    conn, _ = srv.accept()
    conn.close()
    print("dropped", flush=True)
EOF
    standin=$!
    local _; for _ in $(seq 1 20); do [ -S "$sock" ] && break; sleep 0.2; done

    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["pulseaudio"], "pulseaudio": { "format": "{volume}%" }
}
EOF
    PULSE_SERVER="unix:$sock" smoke::launch_waybar "$cfg" "$DIR/style.css" 5
    smoke::assert_alive || fail=1
    local before after; before="$(wc -l <"$hits")"
    sleep 5
    after="$(wc -l <"$hits")"
    echo "waybar reconnected $((after - before)) times in 5 s"
    if [ "$((after - before))" -gt 50 ]; then
        echo "::error::pulseaudio backend spins on a persistent context failure"; fail=1
    fi
    smoke::assert_clean_exit || fail=1   # SIGINT with a reconnect timer pending
    kill "$standin" 2>/dev/null || true
    echo "::endgroup::"
}
pulse_reconnect_tier

[ "$fail" = 0 ] || { echo "::error::state tier failed"; exit 1; }
echo "✓ state-transition tier passed"
