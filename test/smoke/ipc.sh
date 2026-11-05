#!/usr/bin/env bash
# Tier 2: compositor IPC modules against fake IPC servers (fake_ipc.py) that
# send what Hyprland, niri, mango, Umbriel and Wayfire send for an ordinary
# session. Waybar runs under headless sway; each fake only answers its own
# compositor's IPC. Needs a session bus for the AT-SPI assertions:
#   dbus-run-session -- test/smoke/ipc.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    echo "::error::no session bus; run under dbus-run-session"; exit 1
fi

export SMOKE_A11Y=1
smoke::setup
smoke::start_compositor

FAKE_LOG="$(mktemp)"
FAKE_PID=""
cleanup() {
    local st=$?
    if [ "$st" != 0 ]; then
        echo "::group::waybar log"; smoke::log; echo "::endgroup::"
        echo "::group::fake ipc log"; cat "$FAKE_LOG"; echo "::endgroup::"
    fi
    if [ -n "$FAKE_PID" ]; then kill "$FAKE_PID" 2>/dev/null || true; fi
    smoke::stop
}
trap cleanup EXIT

A11Y=(python3 "$DIR/a11y.py" --timeout 10 --showing)

# start_fake COMPOSITOR SOCKET...: serves that compositor's IPC from a coprocess
start_fake() {
    local line=""
    coproc FAKE { exec python3 "$DIR/fake_ipc.py" "$@" 2>>"$FAKE_LOG"; }
    read -r -t 10 -u "${FAKE[0]}" line || true
    [ "$line" = ready ] || { echo "::error::fake $1 ipc did not start"; return 1; }
}

# step 'OP [ARGS]': the fake applies one step of the session and sends its events
step() {
    local ack=""
    echo "$1" >&"${FAKE[1]}"
    read -r -t 10 -u "${FAKE[0]}" ack || true
    [ "$ack" = ok ] || { echo "::error::fake ipc did not apply '$1'"; return 1; }
    echo "step: $1"
}

# launch NAME CONFIG_JSON: starts a bar with that config
launch() {
    local cfg; cfg="$(mktemp --suffix=.json)"
    printf '%s\n' "$2" > "$cfg"
    echo "--- $1"
    smoke::launch_waybar "$cfg" "$DIR/style.css" 2
    smoke::assert_alive
}

# The compositor goes away: the bar keeps its last state, stays up through the
# reconnect attempts, and still exits cleanly. Takes a label that must remain.
compositor_gone() {
    local pid=$FAKE_PID
    echo exit >&"${FAKE[1]}"
    wait "$pid" || true
    FAKE_PID=""
    sleep 3
    smoke::assert_alive
    "${A11Y[@]}" "$1"
    smoke::assert_clean_exit
}

# --- Hyprland: .socket.sock requests, .socket2.sock events ------------------
export HYPRLAND_INSTANCE_SIGNATURE=smoke
hypr="$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE"
mkdir -p "$hypr"
start_fake hyprland "$hypr/.socket.sock" "$hypr/.socket2.sock"
launch hyprland '{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["hyprland/workspaces", "hyprland/submap"],
  "modules-center": ["hyprland/window"],
  "modules-right": ["hyprland/windowcount", "hyprland/language"],
  "hyprland/workspaces": {"format": "ws {name}{icon}",
    "format-icons": {"active": "*", "default": ""}},
  "hyprland/window": {"format": "win: {title}"},
  "hyprland/windowcount": {"format": "count {}"},
  "hyprland/language": {"format": "kb {long}"},
  "hyprland/submap": {"format": "mode {}"}
}'
"${A11Y[@]}" "ws 1*" "ws 2" "win: foot ~" "count 1" "kb English (US)"
step 'workspace 3'
"${A11Y[@]}" "ws 3*" "count 0"
"${A11Y[@]}" --absent "ws 1*" "foot ~"
step 'open kitty "build log"'
"${A11Y[@]}" "win: build log" "count 1"
step 'title "build done"'
"${A11Y[@]}" "win: build done"
"${A11Y[@]}" --absent "build log"
step 'close'
"${A11Y[@]}" "count 0"
"${A11Y[@]}" --absent "build done"
step 'workspace 1'
"${A11Y[@]}" "ws 1*" "win: foot ~" "count 1"
"${A11Y[@]}" --absent "ws 3"
step 'layout'
"${A11Y[@]}" "kb German"
step 'submap resize'
"${A11Y[@]}" "mode resize"
step 'submap'
"${A11Y[@]}" --absent "mode resize"
compositor_gone "ws 1*"
unset HYPRLAND_INSTANCE_SIGNATURE

# --- niri: one socket, "EventStream" ----------------------------------------
# niri keeps one empty workspace last on the output. Leaving an emptied
# workspace, which niri then drops, is left out: once niri/workspaces destroys
# that button, every new AT-SPI client makes the bridge log Atk-CRITICALs.
export NIRI_SOCKET="$XDG_RUNTIME_DIR/niri.sock"
start_fake niri "$NIRI_SOCKET"
launch niri '{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["niri/workspaces"],
  "modules-center": ["niri/window"],
  "modules-right": ["niri/language"],
  "niri/workspaces": {"format": "ws {value}{icon}",
    "format-icons": {"active": "*", "default": ""}},
  "niri/window": {"format": "win: {title}", "show-empty": true},
  "niri/language": {"format": "kb {long}"}
}'
"${A11Y[@]}" "ws 1*" "ws 2" "ws 3" "win: foot ~" "kb English (US)"
step 'workspace 3'
"${A11Y[@]}" "ws 3*"
"${A11Y[@]}" --absent "ws 1*" "foot ~"
step 'open kitty "build log"'
"${A11Y[@]}" "win: build log" "ws 4"
step 'title "build done"'
"${A11Y[@]}" "win: build done"
"${A11Y[@]}" --absent "build log"
step 'workspace 1'
"${A11Y[@]}" "ws 1*" "ws 4" "win: foot ~"
step 'close'
"${A11Y[@]}" "ws 1*" "win: "
"${A11Y[@]}" --absent "foot ~"
step 'layout'
"${A11Y[@]}" "kb German"
compositor_gone "ws 1*"
unset NIRI_SOCKET

# --- mango: "watch all-monitors" snapshots of the nine tags -----------------
export MANGO_INSTANCE_SIGNATURE="$XDG_RUNTIME_DIR/mango.sock"
start_fake mango "$MANGO_INSTANCE_SIGNATURE"
launch mango '{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["mango/workspaces", "mango/keymode"],
  "modules-center": ["mango/window"],
  "modules-right": ["mango/layout", "mango/language"],
  "mango/workspaces": {"format": "ws {name}{icon}",
    "format-icons": {"active": "*", "default": ""}},
  "mango/window": {"format": "win: {title}"},
  "mango/keymode": {"format": "mode {mode}", "format-default": ""},
  "mango/layout": {"format": "tile {symbol}"},
  "mango/language": {"format": "kb {long}"}
}'
"${A11Y[@]}" "ws 1*" "ws 2" "ws 9" "win: foot ~" "tile T" "kb English (US)"
"${A11Y[@]}" --absent "mode"
step 'workspace 3'
"${A11Y[@]}" "ws 3*"
"${A11Y[@]}" --absent "ws 1*" "foot ~"
step 'open kitty "build log"'
"${A11Y[@]}" "win: build log"
step 'title "build done"'
"${A11Y[@]}" "win: build done"
"${A11Y[@]}" --absent "build log"
step 'close'
"${A11Y[@]}" --absent "build done"
step 'workspace 1'
"${A11Y[@]}" "ws 1*" "ws 3" "win: foot ~"
step 'layout'
"${A11Y[@]}" "kb German"
step 'keymode resize'
"${A11Y[@]}" "mode resize"
step 'keymode default'
"${A11Y[@]}" --absent "mode resize"
step 'arrange S'
"${A11Y[@]}" "tile S"
compositor_gone "ws 1*"
unset MANGO_INSTANCE_SIGNATURE

# --- Umbriel: "subscribe" snapshots of workspaces and windows ---------------
export UMBRIEL_SOCKET="$XDG_RUNTIME_DIR/umbriel.sock"
start_fake umbriel "$UMBRIEL_SOCKET"
launch umbriel '{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["umbriel/workspaces"],
  "modules-center": ["umbriel/window"],
  "modules-right": ["umbriel/layout"],
  "umbriel/workspaces": {"format": "ws {value}{icon}",
    "format-icons": {"active": "*", "default": ""}},
  "umbriel/window": {"format": "win: {title}", "show-empty": true},
  "umbriel/layout": {"format": "tile {layout}"}
}'
"${A11Y[@]}" "ws 1*" "ws 2" "ws 3" "win: foot ~" "tile scrolling"
step 'workspace 3'
"${A11Y[@]}" "ws 3*"
"${A11Y[@]}" --absent "ws 1*" "foot ~"
step 'open kitty "build log"'
"${A11Y[@]}" "win: build log"
step 'title "build done"'
"${A11Y[@]}" "win: build done"
"${A11Y[@]}" --absent "build log"
step 'close'
"${A11Y[@]}" --absent "build done"
step 'arrange dwindle'
"${A11Y[@]}" "tile dwindle"
step 'workspace 1'
"${A11Y[@]}" "ws 1*" "ws 3" "win: foot ~" "tile scrolling"
compositor_gone "ws 1*"
unset UMBRIEL_SOCKET

# --- Wayfire: length-prefixed JSON, a fixed 3x1 workspace grid --------------
export WAYFIRE_SOCKET="$XDG_RUNTIME_DIR/wayfire.sock"
start_fake wayfire "$WAYFIRE_SOCKET"
launch wayfire '{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["wayfire/workspaces"],
  "modules-center": ["wayfire/window"],
  "wayfire/workspaces": {"format": "ws {index}{icon}",
    "format-icons": {"focused": "*", "default": ""}},
  "wayfire/window": {"format": "win: {title}"}
}'
"${A11Y[@]}" "ws 1*" "ws 2" "ws 3" "win: foot ~"
step 'workspace 3'
"${A11Y[@]}" "ws 3*"
"${A11Y[@]}" --absent "ws 1*" "foot ~"
step 'open kitty "build log"'
"${A11Y[@]}" "win: build log"
step 'title "build done"'
"${A11Y[@]}" "win: build done"
"${A11Y[@]}" --absent "build log"
step 'close'
"${A11Y[@]}" --absent "build done"
step 'workspace 1'
"${A11Y[@]}" "ws 1*" "ws 3" "win: foot ~"
compositor_gone "ws 1*"
unset WAYFIRE_SOCKET

echo "ipc tier passed"
