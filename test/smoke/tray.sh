#!/usr/bin/env bash
# Tier 4: tray input from session-bus clients. Waybar is the
# StatusNotifierWatcher, so any client can register items with it.
# Needs a session bus and gcc with gio-2.0 for the fake item (tray_item.c):
#   dbus-run-session -- test/smoke/tray.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    echo "::error::no session bus; run under dbus-run-session"; exit 1
fi

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<EOF
{ "layer": "top", "position": "top", "height": 30, "modules-right": ["tray"] }
EOF
smoke::launch_waybar "$cfg" "$DIR/style.css" 4
smoke::assert_alive

WATCHER=(gdbus call --session --dest org.kde.StatusNotifierWatcher
         --object-path /StatusNotifierWatcher)
fail=0

# --- registered service strings are bounded --------------------------------
# Every registration is republished in the RegisteredStatusNotifierItems array,
# whose size limit is below that of a single string argument.
register_item() {
    "${WATCHER[@]}" --method org.kde.StatusNotifierWatcher.RegisterStatusNotifierItem "$1" 2>&1 || true
}
out="$(register_item /StatusNotifierItem)"
if [ "$out" != "()" ]; then
    echo "::error::normal registration refused: $out"; fail=1
fi
long_path="/$(head -c 4096 /dev/zero | tr '\0' a)"
out="$(register_item "$long_path")"
if ! grep -q 'org.freedesktop.DBus.Error.LimitsExceeded' <<<"$out"; then
    echo "::error::oversized registration not refused: ${out:0:200}"; fail=1
fi
sleep 1
smoke::assert_alive || fail=1
out="$("${WATCHER[@]}" --method org.freedesktop.DBus.Properties.Get \
        org.kde.StatusNotifierWatcher RegisteredStatusNotifierItems 2>&1 || true)"
# gdbus disconnects after each call, so the accepted item is gone again by now.
if ! grep -q '@as' <<<"$out" || grep -q 'aaaa' <<<"$out"; then
    echo "::error::unexpected RegisteredStatusNotifierItems: ${out:0:200}"; fail=1
fi
if ! grep -q 'itemRegistered called with service: .*/StatusNotifierItem' "$SMOKE_LOG"; then
    echo "::error::host never saw the accepted item"; fail=1
fi
echo "oversized registration refused, watcher still answers"

# --- oversized dbusmenu layout ----------------------------------------------
# Waybar hands every item's Menu to libdbusmenu, which builds the whole layout
# on the GTK main loop in time quadratic to its size.
item="$(mktemp)"
# shellcheck disable=SC2046
gcc "$DIR/tray_item.c" -o "$item" $(pkg-config --cflags --libs gio-2.0)
"$item" 20000 &
ITEM_PID=$!
trap 'kill $ITEM_PID 2>/dev/null || true; smoke::stop' EXIT
for _ in $(seq 1 20); do
    grep -q "Tray::onAdd" "$SMOKE_LOG" && break
    sleep 0.5
done
# The watcher answers from the main loop that draws the bar: a liveness probe.
for _ in $(seq 1 10); do
    if ! "${WATCHER[@]}" --timeout 5 --method org.freedesktop.DBus.Properties.Get \
            org.kde.StatusNotifierWatcher ProtocolVersion >/dev/null; then
        echo "::error::main loop did not answer within 5 s of a 20000-entry tray menu"
        fail=1
        break
    fi
    sleep 1
done
if ! grep -q "Ignoring the tray menu" "$SMOKE_LOG"; then
    echo "::error::oversized tray menu layout was not refused"; fail=1
fi
kill "$ITEM_PID" 2>/dev/null || true
echo "oversized menu layout refused, main loop stays responsive"

smoke::assert_clean_exit || fail=1
[ "$fail" = 0 ] || { echo "::error::tray tier failed"; exit 1; }
echo "tray tier passed"
