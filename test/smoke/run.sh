#!/usr/bin/env bash
# Launch Waybar inside a headless sway compositor and verify it starts and
# renders. Used by the `smoke` CI workflow.
#
# Usage: run.sh <config> [style] [screenshot.png]
#   - always: assert waybar launches, stays alive, logs no fatal error
#   - if <screenshot.png> given: capture the bar with grim
#
# Requires: sway, grim, waybar (on PATH). Software-rendered, no GPU needed.
set -euo pipefail

CONFIG="$1"
STYLE="${2:-}"
SHOT="${3:-}"

export XDG_RUNTIME_DIR="$(mktemp -d)"
chmod 700 "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1
export LIBGL_ALWAYS_SOFTWARE=1

LOG="$(mktemp)"
SWAYCFG="$(mktemp)"

wb="waybar -c $CONFIG"
[ -n "$STYLE" ] && wb="$wb -s $STYLE"

cat > "$SWAYCFG" <<EOF
output HEADLESS-1 resolution 1920x1080 background #000000 solid_color
exec "$wb -l debug > $LOG 2>&1"
EOF

echo "::group::Starting sway (headless) + waybar"
sway -c "$SWAYCFG" &
SWAY_PID=$!

cleanup() {
    swaymsg -q exit 2>/dev/null || kill "$SWAY_PID" 2>/dev/null || true
}
trap cleanup EXIT

# Wait for the Wayland socket to appear.
sock=""
for _ in $(seq 1 40); do
    sock="$(find "$XDG_RUNTIME_DIR" -maxdepth 1 -name 'wayland-*' ! -name '*.lock' 2>/dev/null | head -1 || true)"
    [ -n "$sock" ] && break
    sleep 0.5
done
[ -n "$sock" ] || { echo "::error::compositor did not create a Wayland socket"; exit 1; }
export WAYLAND_DISPLAY="$(basename "$sock")"
echo "compositor up on WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
echo "::endgroup::"

# Give modules time to poll/settle.
sleep 6

echo "::group::Waybar log"
cat "$LOG" || true
echo "::endgroup::"

# --- Level 1: smoke ---
if ! pgrep -x waybar >/dev/null; then
    echo "::error::waybar is not running — it crashed on startup (config: $CONFIG)"
    exit 1
fi
if grep -iqE 'critical|segfault|\bfatal\b|terminate called' "$LOG"; then
    echo "::error::waybar logged a fatal error (config: $CONFIG)"
    exit 1
fi
echo "✓ waybar launched and stayed alive under headless sway"

# --- Level 2: screenshot ---
if [ -n "$SHOT" ]; then
    grim "$SHOT"
    echo "✓ captured screenshot -> $SHOT"
fi
