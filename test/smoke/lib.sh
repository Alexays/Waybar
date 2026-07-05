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
# Optional command prefix for the waybar launch (e.g. `umockdev-run -d dev --`),
# used by the hardware tier to fake sysfs/udev devices. Empty by default.
WAYBAR_WRAP="${WAYBAR_WRAP:-}"

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
    # Leak detection is opt-in (leakcheck.sh sets SMOKE_DETECT_LEAKS=1) and needs
    # a clean exit for LSan's atexit hook to run.
    export ASAN_OPTIONS="detect_leaks=${SMOKE_DETECT_LEAKS:-0}:halt_on_error=1:abort_on_error=1:detect_odr_violation=0"
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
    local run=()
    [ -n "$WAYBAR_WRAP" ] && read -ra run <<<"$WAYBAR_WRAP"
    run+=("$WAYBAR_BIN" "${args[@]}")
    "${run[@]}" >"$SMOKE_LOG" 2>&1 &
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

# Send a signal to waybar and give it a moment to act on it.
# smoke::signal <SIGNAME|num> [settle_seconds]
smoke::signal() {
    kill -"$1" "$WAYBAR_PID" 2>/dev/null || true
    sleep "${2:-2}"
}

# Reload waybar's config in place (recreates bars + modules).
smoke::reload() { smoke::signal SIGUSR2 "${1:-3}"; }

# Create a headless output at runtime (sway only). wlroots' headless backend
# lets sway hotplug outputs, which makes waybar build a new Bar + module set.
smoke::add_output() {
    [ "$COMPOSITOR" = sway ] || { echo "(add_output: sway only, skipped)"; return 0; }
    swaymsg create_output >/dev/null 2>&1 || echo "(create_output unsupported, skipped)"
    sleep 2
}

# Remove every headless output except the primary HEADLESS-1. Destroying an
# output tears down its Bar -> ~Bar -> GtkWindow unmap -> toggleSuspend(), i.e.
# the exact runtime path that segfaulted in #5182 -- exercised here under ASan.
smoke::remove_output() {
    [ "$COMPOSITOR" = sway ] || return 0
    local o
    for o in $(swaymsg -t get_outputs 2>/dev/null | jq -r '.[].name' 2>/dev/null | grep -v '^HEADLESS-1$' || true); do
        swaymsg output "$o" unplug >/dev/null 2>&1 || true
    done
    sleep 2
}

# Shut waybar down the clean way (SIGINT -> Client::reset -> gtk quit ->
# bars.clear -> ~Bar) and assert the destructor path neither crashed nor tripped
# a sanitizer report. A plain SIGTERM/kill never runs this path, which is why the
# exit-time use-after-free (#5182) went unnoticed. Call instead of smoke::stop's
# kill when you want the teardown itself checked.
smoke::assert_clean_exit() {
    [ -n "$WAYBAR_PID" ] || { echo "(no waybar running)"; return 0; }
    kill -INT "$WAYBAR_PID" 2>/dev/null || true
    local _ ; for _ in $(seq 1 20); do kill -0 "$WAYBAR_PID" 2>/dev/null || break; sleep 0.5; done
    if kill -0 "$WAYBAR_PID" 2>/dev/null; then
        echo "::error::waybar did not exit on SIGINT (hung during teardown)"
        echo "::group::waybar log"; smoke::log; echo "::endgroup::"
        kill -9 "$WAYBAR_PID" 2>/dev/null || true; WAYBAR_PID=""; return 1
    fi
    local st=0; wait "$WAYBAR_PID" 2>/dev/null || st=$?
    WAYBAR_PID=""
    if [ "$st" -ge 128 ]; then
        echo "::error::waybar crashed on exit (killed by signal $((st - 128)))"
        echo "::group::waybar log"; smoke::log; echo "::endgroup::"; return 1
    fi
    smoke::assert_clean || return 1
    echo "✓ clean exit (status $st)"
}

smoke::stop() {
    kill "$WAYBAR_PID" 2>/dev/null || true
    swaymsg -q exit 2>/dev/null || kill "$COMP_PID" 2>/dev/null || true
    wait 2>/dev/null || true
}
