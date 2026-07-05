#!/usr/bin/env bash
# Tier — coverage tripwire. Every module the Factory can build must be classified
# below: either smoke-tested headless (SAFE), or explicitly parked as needing real
# hardware/a daemon (HARDWARE, best-effort in state.sh / hardware.sh) or a specific
# compositor (COMPOSITOR). A newly added module that isn't listed fails this job,
# forcing a conscious "is it covered?" decision instead of silently escaping the
# smoke net -- which is how the slider/backlight crash class (#5179) slipped by.
#
# Usage: coverage.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$DIR/../.." && pwd)"
FACTORY_SRC="$ROOT/src/factory.cpp"

mapfile -t FACTORY < <(grep -oE 'ref == "[^"]+"' "$FACTORY_SRC" | sed -E 's/.*"(.*)"/\1/' | sort -u)
[ "${#FACTORY[@]}" -gt 0 ] || { echo "::error::could not parse module names from $FACTORY_SRC"; exit 1; }

# Rendered headless with no hardware/daemon/compositor dependency.
SAFE=(clock cpu cpu_usage cpu_graph cpu_frequency load memory disk
      idle_inhibitor image user inhibitor)

# Need real hardware or a system daemon; covered best-effort by hardware.sh / state.sh.
HARDWARE=(backlight backlight/slider battery upower power-profiles-daemon bluetooth
          network wwan gps pulseaudio pulseaudio/slider wireplumber jack sndio cava
          mpd mpris temperature keyboard-state systemd-failed-units privacy tray gamemode)

# Need a specific compositor / its IPC to produce output.
COMPOSITOR=(sway/language sway/mode sway/scratchpad sway/window sway/workspaces
            hyprland/language hyprland/submap hyprland/window hyprland/windowcount hyprland/workspaces
            river/layout river/mode river/tags river/window
            niri/language niri/window niri/workspaces
            dwl/tags dwl/window
            mango/keymode mango/language mango/layout mango/window mango/workspaces
            wayfire/window wayfire/workspaces ext/workspaces wlr/taskbar)

declare -A known=()
for m in "${SAFE[@]}" "${HARDWARE[@]}" "${COMPOSITOR[@]}"; do known["$m"]=1; done

missing=()
for m in "${FACTORY[@]}"; do [ -n "${known[$m]:-}" ] || missing+=("$m"); done

if [ "${#missing[@]}" -gt 0 ]; then
    echo "::error::modules not classified for smoke coverage: ${missing[*]}"
    echo "Add each to SAFE / HARDWARE / COMPOSITOR in test/smoke/coverage.sh"
    echo "(and, if it can run headless, to the matrix in modules.sh)."
    exit 1
fi
echo "✓ all ${#FACTORY[@]} factory modules are classified for smoke coverage"
