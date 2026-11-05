#!/usr/bin/env bash
# Tier 2 - compositor modules: drive real windows, workspaces, a binding mode, the scratchpad and
# the keyboard layout through sway's IPC and assert each module's text follows (AT-SPI), then the
# taskbar again under labwc. Must run under a D-Bus session, e.g.:
#   dbus-run-session -- test/smoke/compositor.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

export SMOKE_A11Y=1
# atk-bridge (at-spi2-core 2.62) logs these when an AT-SPI client reads its cache after a button
# holding child widgets is destroyed; a bare GTK3 app does the same, so they are not Waybar's.
export SMOKE_LOG_IGNORE="atk_state_set_contains_state: assertion 'ATK_IS_STATE_SET \(set\)' failed|g_object_unref: assertion 'G_IS_OBJECT \(object\)' failed"
A11Y=(python3 "$DIR/a11y.py" --timeout 8)
WINDOWS=()
KEYBOARD_PID=""

cleanup() {
    kill "${WINDOWS[@]}" $KEYBOARD_PID 2>/dev/null || true
    WINDOWS=()
    smoke::stop
}

fail=0
trap 'echo "::error::command failed at line $LINENO: $BASH_COMMAND"' ERR

echo "::group::sway modules"
export COMPOSITOR=sway
export SWAY_CONFIG_EXTRA=$'mode "smoke-resize" {\n    bindsym Escape mode default\n}'
smoke::setup
smoke::start_compositor
trap cleanup EXIT

# The headless backend has no keyboard: a virtual one gives sway layouts to report.
VK="$(mktemp -d)"
wayland-scanner client-header "$DIR/virtual-keyboard-unstable-v1.xml" \
    "$VK/virtual-keyboard-unstable-v1-client-protocol.h"
wayland-scanner private-code "$DIR/virtual-keyboard-unstable-v1.xml" "$VK/protocol.c"
# shellcheck disable=SC2046
gcc -I"$VK" "$DIR/virtual_keyboard.c" "$VK/protocol.c" -o "$VK/virtual_keyboard" \
    $(pkg-config --cflags --libs wayland-client xkbcommon)
"$VK/virtual_keyboard" us,fr &
KEYBOARD_PID=$!

# open_window TITLE: start a foot window and wait until sway maps it
open_window() {
    foot -T "$1" -a "${1,,}" sleep 600 >/dev/null 2>&1 &
    WINDOWS+=($!)
    local _
    for _ in $(seq 1 40); do
        swaymsg -t get_tree | jq -e --arg t "$1" '.. | objects | select(.name? == $t)' >/dev/null && return 0
        sleep 0.25
    done
    echo "::error::window $1 did not map"; return 1
}

# ext/workspaces needs ext-workspace-v1, which older sway releases do not offer
EXT=0
wayland-info -i ext_workspace_manager_v1 2>/dev/null | grep -q ext_workspace_manager_v1 && EXT=1
[ "$EXT" = 1 ] || echo "(sway does not offer ext-workspace-v1: ext/workspaces assertions skipped)"

swaymsg -q workspace SMOKE-WS-A
open_window SMOKE-ONE

cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["sway/workspaces", "sway/mode", "sway/scratchpad", "ext/workspaces"],
  "modules-center": ["sway/window"],
  "modules-right": ["sway/language", "wlr/taskbar"],
  "sway/workspaces": { "format": "WS:{name}" },
  "sway/mode": { "format": "MODE:{}" },
  "sway/scratchpad": { "format": "SCRATCH:{count}", "show-empty": true },
  "sway/window": { "format": "WIN:{title}" },
  "sway/language": { "format": "LANG:{short}" },
  "wlr/taskbar": { "format": "TASK:{title}" },
  "ext/workspaces": { "format": "EXT:{name}" }
}
EOF
smoke::launch_waybar "$cfg" "$DIR/style.css" 4
smoke::assert_alive || fail=1
"${A11Y[@]}" WS:SMOKE-WS-A WIN:SMOKE-ONE SCRATCH:0 LANG:us TASK:SMOKE-ONE || fail=1
"${A11Y[@]}" --absent MODE: || fail=1
[ "$EXT" = 0 ] || "${A11Y[@]}" EXT:SMOKE-WS-A || fail=1

open_window SMOKE-TWO
"${A11Y[@]}" WIN:SMOKE-TWO TASK:SMOKE-ONE TASK:SMOKE-TWO || fail=1
"${A11Y[@]}" --absent WIN:SMOKE-ONE || fail=1

swaymsg -q workspace SMOKE-WS-B
"${A11Y[@]}" WS:SMOKE-WS-A WS:SMOKE-WS-B || fail=1
"${A11Y[@]}" --absent WIN:SMOKE-TWO || fail=1
[ "$EXT" = 0 ] || "${A11Y[@]}" EXT:SMOKE-WS-A EXT:SMOKE-WS-B || fail=1

swaymsg -q rename workspace SMOKE-WS-A to SMOKE-WS-C
"${A11Y[@]}" WS:SMOKE-WS-C || fail=1
"${A11Y[@]}" --absent WS:SMOKE-WS-A || fail=1
[ "$EXT" = 0 ] || "${A11Y[@]}" EXT:SMOKE-WS-C || fail=1

# Leaving the empty workspace destroys it
swaymsg -q workspace SMOKE-WS-C
"${A11Y[@]}" WIN:SMOKE-TWO || fail=1
"${A11Y[@]}" --absent WS:SMOKE-WS-B || fail=1
[ "$EXT" = 0 ] || "${A11Y[@]}" --absent EXT:SMOKE-WS-B || fail=1

swaymsg -q mode smoke-resize
"${A11Y[@]}" MODE:smoke-resize || fail=1
swaymsg -q mode default
"${A11Y[@]}" --absent MODE:smoke-resize || fail=1

swaymsg -q '[title="SMOKE-TWO"] move scratchpad'
"${A11Y[@]}" SCRATCH:1 WIN:SMOKE-ONE || fail=1
"${A11Y[@]}" --absent SCRATCH:0 || fail=1
swaymsg -q '[title="SMOKE-TWO"] scratchpad show'
"${A11Y[@]}" SCRATCH:0 WIN:SMOKE-TWO || fail=1
"${A11Y[@]}" --absent SCRATCH:1 || fail=1

swaymsg -q input type:keyboard xkb_switch_layout next
"${A11Y[@]}" LANG:fr || fail=1
"${A11Y[@]}" --absent LANG:us || fail=1

swaymsg -q '[title="SMOKE-ONE"] kill'
"${A11Y[@]}" TASK:SMOKE-TWO || fail=1
"${A11Y[@]}" --absent TASK:SMOKE-ONE || fail=1

smoke::assert_clean_exit || fail=1
cleanup
echo "::endgroup::"

echo "::group::wlr/taskbar under labwc"
export COMPOSITOR=labwc
smoke::setup
smoke::start_compositor
foot -T SMOKE-LABWC-ONE sleep 600 >/dev/null 2>&1 &
WINDOWS+=($!)
foot -T SMOKE-LABWC-TWO sleep 600 >/dev/null 2>&1 &
WINDOWS+=($!)
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["wlr/taskbar"],
  "wlr/taskbar": { "format": "TASK:{title}" }
}
EOF
smoke::launch_waybar "$cfg" "$DIR/style.css" 4
smoke::assert_alive || fail=1
"${A11Y[@]}" TASK:SMOKE-LABWC-ONE TASK:SMOKE-LABWC-TWO || fail=1
kill "${WINDOWS[0]}"
"${A11Y[@]}" TASK:SMOKE-LABWC-TWO || fail=1
"${A11Y[@]}" --absent TASK:SMOKE-LABWC-ONE || fail=1
foot -T SMOKE-LABWC-THREE sleep 600 >/dev/null 2>&1 &
WINDOWS+=($!)
"${A11Y[@]}" TASK:SMOKE-LABWC-THREE TASK:SMOKE-LABWC-TWO || fail=1
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

[ "$fail" = 0 ] || exit 1
echo "✓ compositor module tests passed"
