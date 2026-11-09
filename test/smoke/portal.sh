#!/usr/bin/env bash
# Tier: desktop portal peer. A malformed SettingChanged signal from the owner of
# org.freedesktop.portal.Desktop must be ignored, not abort the bar.
# Needs a private session bus with no real portal on it:
#   dbus-run-session -- test/smoke/portal.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

peer="$(mktemp)"
# shellcheck disable=SC2046
cc -o "$peer" "$DIR/portal_peer.c" $(pkg-config --cflags --libs gio-2.0)

smoke::setup
smoke::start_compositor
trap 'smoke::stop; rm -f "$peer"' EXIT
smoke::launch_waybar "$DIR/config.jsonc" "$DIR/style.css" 6
smoke::assert_alive

# The real portal's shape first, then wrong argument types, a value that is not
# a variant, a variant of the wrong inner type, and the wrong argument count.
"$peer" \
    '("org.freedesktop.appearance", "color-scheme", <uint32 1>)' \
    '(uint32 1, uint32 2, <uint32 1>)' \
    '("org.freedesktop.appearance", "color-scheme", uint32 1)' \
    '("org.freedesktop.appearance", "color-scheme", <"dark">)' \
    '("org.freedesktop.appearance", "color-scheme")'
sleep 2

fail=0
if grep -q "Received new appearance 'dark'" "$SMOKE_LOG"; then
    echo "OK: waybar followed the well-formed SettingChanged signal"
else
    echo "::error::waybar did not react to the well-formed SettingChanged signal"
    echo "::group::waybar log"; smoke::log; echo "::endgroup::"
    fail=1
fi
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1

[ "$fail" = 0 ] || { echo "::error::portal tier failed"; exit 1; }
echo "OK: portal peer smoke passed"
