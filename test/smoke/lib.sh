#!/usr/bin/env bash
# Shared helpers for the headless Waybar smoke/render/interaction tests.
#
# A test sources this file, then typically:
#   smoke::setup
#   smoke::start_compositor
#   smoke::launch_waybar <config> [style]
#   smoke::assert_alive
#   smoke::assert_clean            # ASan/UBSan/Gtk-CRITICAL/segfault gate
#   smoke::screenshot out.png
#   smoke::stop
#
# Environment knobs (optional): WIDTH HEIGHT SCALE COMPOSITOR WAYBAR_BIN
set -uo pipefail

WIDTH="${WIDTH:-1920}"
HEIGHT="${HEIGHT:-1080}"
SCALE="${SCALE:-1}"
COMPOSITOR="${COMPOSITOR:-sway}"
WAYBAR_BIN="${WAYBAR_BIN:-waybar}"

SMOKE_LOG=""
COMP_PID=""
WAYBAR_PID=""

smoke::setup() {
    export XDG_RUNTIME_DIR="$(mktemp -d)"
    chmod 700 "$XDG_RUNTIME_DIR"
    export WLR_BACKENDS=headless
    export WLR_RENDERER=pixman
    export WLR_LIBINPUT_NO_DEVICES=1
    export LIBGL_ALWAYS_SOFTWARE=1
    # Keep ASan/UBSan noise low but fatal on real errors (leaks in GTK/glib are
    # too noisy to gate on, so only memory-corruption and UB abort the run).
    export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1:detect_odr_violation=0"
    export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
}

smoke::start_compositor() {
    case "$COMPOSITOR" in
        sway)
            local cfg; cfg="$(mktemp)"
            cat > "$cfg" <<EOF
output HEADLESS-1 resolution ${WIDTH}x${HEIGHT} scale ${SCALE} background #000000 solid_color
EOF
            sway -c "$cfg" >/tmp/compositor.log 2>&1 &
            COMP_PID=$!
            ;;
        labwc)
            # labwc reads outputs from the wlroots headless backend directly.
            labwc >/tmp/compositor.log 2>&1 &
            COMP_PID=$!
            ;;
        *)
            echo "::error::unknown compositor '$COMPOSITOR'"; return 1 ;;
    esac

    local sock=""
    for _ in $(seq 1 40); do
        sock="$(find "$XDG_RUNTIME_DIR" -maxdepth 1 -name 'wayland-*' ! -name '*.lock' 2>/dev/null | head -1 || true)"
        [ -n "$sock" ] && break
        sleep 0.5
    done
    if [ -z "$sock" ]; then
        echo "::error::$COMPOSITOR did not create a Wayland socket"; cat /tmp/compositor.log || true; return 1
    fi
    export WAYLAND_DISPLAY="$(basename "$sock")"

    # sway IPC socket, so swaymsg (cursor injection, clean exit) can reach it.
    if [ "$COMPOSITOR" = sway ]; then
        for _ in $(seq 1 10); do
            SWAYSOCK="$(find "$XDG_RUNTIME_DIR" -maxdepth 1 -name 'sway-ipc.*.sock' 2>/dev/null | head -1 || true)"
            [ -n "$SWAYSOCK" ] && break
            sleep 0.3
        done
        export SWAYSOCK
    fi
    echo "compositor '$COMPOSITOR' up on WAYLAND_DISPLAY=$WAYLAND_DISPLAY (${WIDTH}x${HEIGHT} scale ${SCALE})"
}

# smoke::launch_waybar <config> [style] [settle_seconds]
smoke::launch_waybar() {
    local config="$1" style="${2:-}" settle="${3:-6}"
    SMOKE_LOG="$(mktemp)"
    local args=(-c "$config" -l debug)
    [ -n "$style" ] && args+=(-s "$style")
    "$WAYBAR_BIN" "${args[@]}" >"$SMOKE_LOG" 2>&1 &
    WAYBAR_PID=$!
    sleep "$settle"
}

smoke::log() { cat "$SMOKE_LOG" 2>/dev/null || true; }

smoke::assert_alive() {
    if ! kill -0 "$WAYBAR_PID" 2>/dev/null; then
        echo "::error::waybar exited early (crashed on startup)"
        echo "::group::waybar log"; smoke::log; echo "::endgroup::"
        return 1
    fi
    echo "✓ waybar is alive"
}

# Fail on sanitizer reports and GTK/GLib criticals in the log.
smoke::assert_clean() {
    local bad
    bad="$(grep -iE 'AddressSanitizer|runtime error:|LeakSanitizer|Gtk-CRITICAL|GLib-CRITICAL|GLib-GObject-CRITICAL|assertion .*failed|segfault|terminate called|SUMMARY: .*Sanitizer' "$SMOKE_LOG" || true)"
    if [ -n "$bad" ]; then
        echo "::error::waybar reported sanitizer/critical issues:"
        echo "$bad"
        echo "::group::full waybar log"; smoke::log; echo "::endgroup::"
        return 1
    fi
    echo "✓ no sanitizer/critical issues in log"
}

smoke::screenshot() {
    local out="$1"
    grim "$out"
    echo "✓ screenshot -> $out"
}

# Assert the top bar strip actually rendered something (not a uniform color).
smoke::assert_not_blank() {
    local img="$1" h="${2:-30}"
    local sd
    sd="$(convert "$img" -crop "${WIDTH}x${h}+0+0" +repage -colorspace Gray -format '%[fx:standard_deviation]' info:)"
    echo "bar strip stddev = $sd"
    awk "BEGIN{ exit !($sd > 0.01) }" || { echo "::error::bar strip is blank in $img"; return 1; }
    echo "✓ bar rendered content"
}

smoke::stop() {
    kill "$WAYBAR_PID" 2>/dev/null || true
    swaymsg -q exit 2>/dev/null || kill "$COMP_PID" 2>/dev/null || true
    wait 2>/dev/null || true
}
