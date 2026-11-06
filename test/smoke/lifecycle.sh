#!/usr/bin/env bash
# Tier 4 — lifecycle / teardown safety net.
#
# Everything above renders in steady state and kills waybar with SIGTERM, which
# never runs the C++ destructors. The crash classes that hurt most (exit,
# DPMS/output unmap, reload) all live in that create/destroy path, so exercise it
# explicitly under ASan:
#   1. clean exit on SIGINT (the real Bar teardown path, #5182)
#   2. runtime output hotplug (bar + module destruction while running, #5182)
#   3. visibility toggle + config reload churn (SIGUSR1 / SIGUSR2)
#   4. fast-interval modules torn down mid-tick (thread vs destructor races)
#
# Usage: lifecycle.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

CFG="$DIR/config.jsonc"
STYLE="$DIR/style.css"

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

fail=0

# --- 1) clean exit on SIGINT (destructor path, #5182) --------------------
echo "::group::clean exit (SIGINT)"
smoke::launch_waybar "$CFG" "$STYLE" 5
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

# --- 2) output hotplug at runtime (bar/module destroy, #5182) ------------
if [ "$COMPOSITOR" = sway ]; then
    echo "::group::output hotplug"
    smoke::launch_waybar "$CFG" "$STYLE" 5
    smoke::assert_alive || fail=1
    for _ in 1 2 3; do
        smoke::add_output
        smoke::assert_alive || { fail=1; break; }
        smoke::remove_output   # destroys that output's Bar -> ~Bar -> unmap
        smoke::assert_alive || { fail=1; break; }
    done
    smoke::assert_clean || fail=1
    smoke::assert_clean_exit || fail=1
    echo "::endgroup::"
fi

# --- 3) visibility toggle + reload churn ---------------------------------
echo "::group::signal churn (toggle + reload)"
smoke::launch_waybar "$CFG" "$STYLE" 5
smoke::assert_alive || fail=1
for _ in 1 2 3; do smoke::signal SIGUSR1 1; done   # hide/show mode churn
smoke::reload                                      # SIGUSR2: recreate bars/modules
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

# --- 4) fast-interval modules torn down mid-tick -------------------------
echo "::group::fast-interval teardown race"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["clock", "cpu", "memory", "custom/tick"],
  "clock": { "interval": 1 },
  "cpu": { "interval": 1 },
  "memory": { "interval": 1 },
  "custom/tick": { "exec": "date +%s", "interval": 1 }
}
EOF
smoke::launch_waybar "$cfg" "$STYLE" 8
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1   # SIGINT while worker threads are mid-tick
echo "::endgroup::"

# --- keyboard-state: descriptors stay flat across reloads -----------------
# Without a usable keyboard (as in CI) the module constructor throws; nothing it
# opened may stay behind, or every reload leaks descriptors until EMFILE.
echo "::group::keyboard-state descriptor count across reloads"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["keyboard-state", "clock"],
  "keyboard-state": { "numlock": true, "capslock": true }
}
EOF
smoke::launch_waybar "$cfg" "$STYLE" 5
smoke::assert_alive || fail=1
before="$(find "/proc/$WAYBAR_PID/fd" -mindepth 1 | wc -l)"
for _ in 1 2 3; do smoke::reload 5; done
after="$(find "/proc/$WAYBAR_PID/fd" -mindepth 1 | wc -l)"
if [ "$after" -gt "$before" ]; then
    echo "::error::open descriptors grew from $before to $after over 3 reloads"; fail=1
fi
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

# --- pulseaudio constructor failure on every reload (descriptor leak) -----
# PULSE_SERVER on a socket nobody listens on makes pa_context_connect() fail
# synchronously, so the module constructor throws on each reload; what it
# acquired must not pile up.
echo "::group::failed pulseaudio construction on reload (fd leak)"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["pulseaudio"]
}
EOF
PULSE_SERVER="unix:$(mktemp -d)/no-such-socket" smoke::launch_waybar "$cfg" "$STYLE" 5
smoke::assert_alive || fail=1
grep -q 'pa_context_connect() failed' "$SMOKE_LOG" \
    || echo "(pulseaudio connected or was not built in; leak check is vacuous)"
fds_before="$(ls "/proc/$WAYBAR_PID/fd" | wc -l)"
for _ in $(seq 1 10); do smoke::reload 1; done
fds_after="$(ls "/proc/$WAYBAR_PID/fd" | wc -l)"
if [ "$fds_after" -gt "$((fds_before + 5))" ]; then
    echo "::error::descriptors leaked across 10 reloads: $fds_before -> $fds_after"
    fail=1
else
    echo "descriptor count stable across reloads ($fds_before -> $fds_after)"
fi
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

# --- registry listener outliving its module -----------------------------
# The connection survives a reload: a later global must not reach a destroyed module.
if [ "$COMPOSITOR" = sway ]; then
    echo "::group::new global after reload (registry listener lifetime)"
    cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["wlr/taskbar"]
}
EOF
    smoke::launch_waybar "$cfg" "$STYLE" 5
    smoke::assert_alive || fail=1
    smoke::reload
    smoke::assert_alive || fail=1
    swaymsg seat seat-extra fallback false >/dev/null 2>&1 || true
    sleep 3
    smoke::assert_alive || fail=1
    smoke::assert_clean || fail=1
    smoke::assert_clean_exit || fail=1
    echo "::endgroup::"
fi

# --- reload of two instances sharing one session bus ----------------------
# Each reload runs a new Gtk::Application; the instance that does not own the
# bus name must survive it.
if command -v dbus-daemon >/dev/null 2>&1; then
    echo "::group::reload of a second instance on the session bus"
    addr_file="$(mktemp)"; pid_file="$(mktemp)"
    dbus-daemon --session --fork --print-address=3 --print-pid=4 3>"$addr_file" 4>"$pid_file"
    bus_pid="$(cat "$pid_file")"
    export DBUS_SESSION_BUS_ADDRESS="$(cat "$addr_file")"
    smoke::launch_waybar "$CFG" "$STYLE" 5
    first_pid=$WAYBAR_PID; first_log=$SMOKE_LOG
    smoke::launch_waybar "$CFG" "$STYLE" 5
    smoke::assert_alive || fail=1
    smoke::reload
    smoke::assert_alive || fail=1
    smoke::assert_clean_exit || fail=1
    WAYBAR_PID=$first_pid; SMOKE_LOG=$first_log
    smoke::reload
    smoke::assert_alive || fail=1
    smoke::assert_clean_exit || fail=1
    kill "$bus_pid" 2>/dev/null || true
    unset DBUS_SESSION_BUS_ADDRESS
    echo "::endgroup::"
fi

# --- 5) tray torn down before its bus connection arrived -----------------
# Without a session bus the SNI host and watcher never get busAcquired(), so
# reload and exit destroy a host with no name watch and an unexported watcher.
echo "::group::tray teardown without a session bus"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["tray"]
}
EOF
WAYBAR_WRAP="env -u DBUS_SESSION_BUS_ADDRESS" smoke::launch_waybar "$cfg" "$STYLE" 5
smoke::assert_alive || fail=1
smoke::reload
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1
if grep -q 'g_bus_unwatch_name' "$SMOKE_LOG"; then
    echo "::error::SNI host passed an unassigned id to g_bus_unwatch_name()"
    fail=1
fi
echo "::endgroup::"

# --- 5) bars created while the idle inhibitor is active ------------------
# idle_inhibitor needs Bar::surface in its first update(); a null one kills the connection.
echo "::group::idle inhibitor active at bar creation"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["idle_inhibitor"],
  "idle_inhibitor": { "start-activated": true }
}
EOF
smoke::launch_waybar "$cfg" "$STYLE" 5
smoke::assert_alive || fail=1
smoke::add_output                                  # new Bar, inhibitor still on (sway only)
smoke::assert_alive || fail=1
smoke::reload                                      # the status survives a reload
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

[ "$fail" = 0 ] || { echo "::error::lifecycle tier failed"; exit 1; }
echo "✓ lifecycle tier passed"
