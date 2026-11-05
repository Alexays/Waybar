#!/usr/bin/env bash
# Tier 5: modules backed by D-Bus services, faked with python-dbusmock. The
# system services run on a private bus, so nothing reaches the host's daemons.
# Each state change is asserted on the text the module shows, and every service
# goes away and comes back while the bar keeps running.
# Needs a session bus (for the AT-SPI tree, gamemode and mpris):
#   dbus-run-session -- test/smoke/dbus.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    echo "::error::no session bus; run under dbus-run-session"; exit 1
fi

export SMOKE_A11Y=1
smoke::setup

MOCK_LOG="$(mktemp)"
declare -A MOCK_PID=()
SYSBUS_PID=""
cleanup() {
    local p
    for p in "${MOCK_PID[@]}" $SYSBUS_PID; do kill "$p" 2>/dev/null || true; done
    smoke::stop
}
trap cleanup EXIT

# A session-type daemon is enough to stand in for the system bus.
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/system_bus_socket"
dbus-daemon --session --nofork --nopidfile --address="$DBUS_SYSTEM_BUS_ADDRESS" &
SYSBUS_PID=$!
for _ in $(seq 1 20); do
    [ -S "$XDG_RUNTIME_DIR/system_bus_socket" ] && break
    sleep 0.25
done

# mock <bus name> <system|session> <dbusmock args...>: (re)start a fake service
# and wait until it owns its name.
mock() {
    local name="$1" bus="$2"; shift 2
    if [ -n "${MOCK_PID[$name]:-}" ]; then
        kill "${MOCK_PID[$name]}" 2>/dev/null || true
        wait "${MOCK_PID[$name]}" 2>/dev/null || true
    fi
    python3 -m dbusmock "--$bus" "$@" >>"$MOCK_LOG" 2>&1 &
    MOCK_PID[$name]=$!
    if ! gdbus wait "--$bus" --timeout 10 "$name"; then
        echo "::error::fake $name did not come up"; cat "$MOCK_LOG"; exit 1
    fi
}

# vanish <bus name>: stop a fake service.
vanish() {
    kill "${MOCK_PID[$1]}"
    wait "${MOCK_PID[$1]}" 2>/dev/null || true
    unset "MOCK_PID[$1]"
}

MOCK=org.freedesktop.DBus.Mock
PROPS=org.freedesktop.DBus.Properties
UP=(gdbus call --system -d org.freedesktop.UPower -o /org/freedesktop/UPower -m)
UP_DISPLAY=/org/freedesktop/UPower/devices/DisplayDevice
PPD=(gdbus call --system -d net.hadess.PowerProfiles -o /net/hadess/PowerProfiles -m)
BLUEZ=(gdbus call --system -d org.bluez -o / -m)
HCI0_POWERED=(gdbus call --system -d org.bluez -o /org/bluez/hci0 -m "$PROPS.Set"
              org.bluez.Adapter1 Powered)
SD=(gdbus call --system -d org.freedesktop.systemd1 -o /org/freedesktop/systemd1 -m)
MM=(gdbus call --system -d org.freedesktop.ModemManager1 -o /org/freedesktop/ModemManager1 -m)
# dbusmock's UpdateProperties cannot wrap structs and dicts, Properties.Set can.
MODEM_SET=(gdbus call --system -d org.freedesktop.ModemManager1
           -o /org/freedesktop/ModemManager1/Modem/0 -m "$PROPS.Set"
           org.freedesktop.ModemManager1.Modem)
INHIBITORS=(gdbus call --system -d org.freedesktop.login1 -o /org/freedesktop/login1
            -m org.freedesktop.login1.Manager.ListInhibitors)
GM=(gdbus call --session -d com.feralinteractive.GameMode -o /com/feralinteractive/GameMode -m)
PLAYER=(gdbus call --session -d org.mpris.MediaPlayer2.smoke -o /org/mpris/MediaPlayer2 -m)
PLAYER_IFACE=org.mpris.MediaPlayer2.Player
PLAYER_SET=(gdbus call --session -d org.mpris.MediaPlayer2.smoke -o /org/mpris/MediaPlayer2
            -m "$PROPS.Set" "$PLAYER_IFACE")

start_upower() {
    mock org.freedesktop.UPower system -t upower
    # Type battery, state discharging, percentage, energy, full, rate, time to empty and full
    "${UP[@]}" "$MOCK.SetupDisplayDevice" 2 2 "$1" 50.0 100.0 10.0 3600 0 true '' 1 >/dev/null
}

start_systemd() {
    mock org.freedesktop.systemd1 system org.freedesktop.systemd1 /org/freedesktop/systemd1 \
        org.freedesktop.systemd1.Manager
    "${SD[@]}" "$MOCK.AddProperties" org.freedesktop.systemd1.Manager \
        "{'SystemState': <'running'>, 'NFailedUnits': <uint32 0>}" >/dev/null
    "${SD[@]}" "$MOCK.AddMethod" org.freedesktop.systemd1.Manager ListUnits '' 'a(ssssssouso)' \
        "ret = [('smoke.service', 'Smoke unit', 'loaded', 'failed', 'failed', '', '/org/freedesktop/systemd1/unit/smoke_2eservice', 0, '', '/')]" \
        >/dev/null
}

start_modemmanager() {
    mock org.freedesktop.ModemManager1 system -m org.freedesktop.ModemManager1 \
        /org/freedesktop/ModemManager1 org.freedesktop.ModemManager1
    # State 8 is registered; modes 3G|4G allowed, 4G preferred; power state 3 is on.
    "${MM[@]}" "$MOCK.AddObject" /org/freedesktop/ModemManager1/Modem/0 \
        org.freedesktop.ModemManager1.Modem \
        "{'State': <int32 8>, 'SignalQuality': <(uint32 $1, true)>,
          'CurrentModes': <(uint32 12, uint32 8)>, 'PowerState': <uint32 3>,
          'EquipmentIdentifier': <'490154203237518'>, 'Sim': <objectpath '/'>}" \
        '@a(ssss) []' >/dev/null
}

start_gamemode() {
    mock com.feralinteractive.GameMode session com.feralinteractive.GameMode \
        /com/feralinteractive/GameMode com.feralinteractive.GameMode
    "${GM[@]}" "$MOCK.AddProperty" com.feralinteractive.GameMode ClientCount '<int32 0>' >/dev/null
}

# start_player <title>
start_player() {
    mock org.mpris.MediaPlayer2.smoke session org.mpris.MediaPlayer2.smoke \
        /org/mpris/MediaPlayer2 "$PLAYER_IFACE"
    "${PLAYER[@]}" "$MOCK.AddProperties" org.mpris.MediaPlayer2 \
        "{'Identity': <'Smoke'>, 'CanQuit': <false>, 'CanRaise': <false>,
          'HasTrackList': <false>}" >/dev/null
    "${PLAYER[@]}" "$MOCK.AddProperties" "$PLAYER_IFACE" \
        "{'PlaybackStatus': <'Stopped'>, 'Metadata': <@a{sv} {}>, 'Position': <int64 0>,
          'Rate': <1.0>, 'Volume': <1.0>, 'CanControl': <true>, 'CanPlay': <true>,
          'CanPause': <true>, 'CanGoNext': <true>, 'CanGoPrevious': <true>,
          'CanSeek': <false>}" >/dev/null
    "${PLAYER[@]}" "$MOCK.AddMethod" "$PLAYER_IFACE" PlayPause '' '' \
        "self.UpdateProperties('$PLAYER_IFACE', {'PlaybackStatus': 'Paused' if self.Get('$PLAYER_IFACE', 'PlaybackStatus') == 'Playing' else 'Playing'})" \
        >/dev/null
    # Set, not added: the player's proxy may have loaded its properties before they existed.
    "${PLAYER_SET[@]}" Metadata "<{'mpris:trackid': <objectpath '/smoke/1'>,
        'xesam:title': <'$1'>, 'xesam:artist': <['Smoke Artist']>,
        'mpris:length': <int64 180000000>}>" >/dev/null
    "${PLAYER_SET[@]}" PlaybackStatus "<'Playing'>" >/dev/null
}

start_upower 42.0
mock net.hadess.PowerProfiles system -t power_profiles_daemon
mock org.bluez system -t bluez5
"${BLUEZ[@]}" org.bluez.Mock.AddAdapter hci0 smoke-host >/dev/null
mock org.freedesktop.login1 system -t logind
start_systemd
start_modemmanager 70
start_gamemode
start_player "First Song"
echo "✓ fake services up"

smoke::start_compositor

cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<'EOF'
{
  "layer": "top", "position": "top", "height": 30,
  "modules-left": ["inhibitor", "upower", "upower#mouse", "bluetooth", "systemd-failed-units"],
  "modules-center": ["power-profiles-daemon"],
  "modules-right": ["wwan", "gamemode", "mpris"],
  "inhibitor": { "what": "idle", "format": "INH {status}" },
  "upower": { "format": "UP {percentage} {time}", "show-icon": false },
  "upower#mouse": { "model": "Smoke Mouse", "format": "MOUSE {percentage}",
                    "show-icon": false, "hide-if-empty": false },
  "bluetooth": { "format": "BT {status}", "format-connected": "BT {status} {device_alias}" },
  "systemd-failed-units": { "user": false, "hide-on-ok": false,
                            "format": "SD {nr_failed} failed", "format-ok": "SD ok" },
  "power-profiles-daemon": { "format": "PPD {profile}" },
  "wwan": { "interval": 1, "hide-disconnected": false,
            "format": "WWAN {state} {signal_quality}% {preferred_mode}" },
  "gamemode": { "format": "GM [{count}]", "hide-not-running": false, "use-icon": false },
  "mpris": { "player": "smoke", "format-playing": "PLAYING {artist} - {title}",
             "format-paused": "PAUSED {title}" }
}
EOF
smoke::launch_waybar "$cfg" "$DIR/style.css" 3
smoke::assert_alive

A11Y=(python3 "$DIR/a11y.py" --timeout 10)
fail=0

"${A11Y[@]}" "INH deactivated" "UP 42% 1h 0min" "BT on" "SD ok" "PPD balanced" \
    "WWAN Registered 70% 4G" "GM []" "PLAYING Smoke Artist - First Song" || fail=1

# The headless backend has no input device: a virtual pointer gives the seat one.
CLICK="$(mktemp -d)"
wayland-scanner client-header "$DIR/wlr-virtual-pointer-unstable-v1.xml" \
    "$CLICK/wlr-virtual-pointer-unstable-v1-client-protocol.h"
wayland-scanner private-code "$DIR/wlr-virtual-pointer-unstable-v1.xml" "$CLICK/protocol.c"
# shellcheck disable=SC2046
gcc -I"$CLICK" "$DIR/pointer_click.c" "$CLICK/protocol.c" -o "$CLICK/pointer_click" \
    $(pkg-config --cflags --libs wayland-client)
# The inhibitor is the leftmost module, the PPD one the only centered one and mpris the rightmost.
CLICK_INHIBITOR=("$CLICK/pointer_click" 10 15 "$WIDTH" "$HEIGHT")
CLICK_PPD=("$CLICK/pointer_click" $((WIDTH / 2)) 15 "$WIDTH" "$HEIGHT")
CLICK_MPRIS=("$CLICK/pointer_click" $((WIDTH - 10)) 15 "$WIDTH" "$HEIGHT")

echo "::group::upower"
"${UP[@]}" "$MOCK.SetDeviceProperties" "$UP_DISPLAY" \
    "{'State': <uint32 1>, 'Percentage': <80.0>, 'TimeToFull': <int64 1800>}" >/dev/null
"${A11Y[@]}" "UP 80% 30min" || fail=1
"${UP[@]}" "$MOCK.AddDischargingBattery" mouse "Smoke Mouse" 55.0 600 >/dev/null
"${A11Y[@]}" "MOUSE 55%" || fail=1
"${UP[@]}" "$MOCK.RemoveDevice" /org/freedesktop/UPower/devices/mouse >/dev/null
"${A11Y[@]}" --absent "MOUSE" || fail=1
vanish org.freedesktop.UPower
sleep 1
smoke::assert_alive || fail=1
start_upower 33.0
"${A11Y[@]}" "UP 33% 1h 0min" || fail=1
echo "::endgroup::"

echo "::group::power-profiles-daemon"
"${PPD[@]}" "$PROPS.Set" net.hadess.PowerProfiles ActiveProfile "<'performance'>" >/dev/null
"${A11Y[@]}" "PPD performance" || fail=1
# A left click moves to the next profile, wrapping around to the first.
"${CLICK_PPD[@]}"
"${A11Y[@]}" "PPD power-saver" || fail=1
if ! "${PPD[@]}" "$PROPS.Get" net.hadess.PowerProfiles ActiveProfile | grep -q "'power-saver'"; then
    echo "::error::the click did not set the power-saver profile on the daemon"; fail=1
fi
vanish net.hadess.PowerProfiles
sleep 1
smoke::assert_alive || fail=1
mock net.hadess.PowerProfiles system -t power_profiles_daemon -p '{"ActiveProfile": "performance"}'
"${A11Y[@]}" "PPD performance" || fail=1
echo "::endgroup::"

echo "::group::bluetooth"
"${HCI0_POWERED[@]}" '<false>' >/dev/null
"${A11Y[@]}" "BT off" || fail=1
"${HCI0_POWERED[@]}" '<true>' >/dev/null
"${A11Y[@]}" "BT on" || fail=1
"${BLUEZ[@]}" org.bluez.Mock.AddDevice hci0 11:22:33:44:55:66 "Smoke Headset" >/dev/null
"${BLUEZ[@]}" org.bluez.Mock.ConnectDevice hci0 11:22:33:44:55:66 >/dev/null
"${A11Y[@]}" "BT connected Smoke Headset" || fail=1
"${BLUEZ[@]}" org.bluez.Mock.DisconnectDevice hci0 11:22:33:44:55:66 >/dev/null
"${A11Y[@]}" "BT on" || fail=1
vanish org.bluez
"${A11Y[@]}" "BT no-controller" || fail=1
smoke::assert_alive || fail=1
mock org.bluez system -t bluez5
"${BLUEZ[@]}" org.bluez.Mock.AddAdapter hci0 smoke-host >/dev/null
"${A11Y[@]}" "BT on" || fail=1
echo "::endgroup::"

echo "::group::systemd-failed-units"
"${SD[@]}" "$MOCK.UpdateProperties" org.freedesktop.systemd1.Manager \
    "{'SystemState': <'degraded'>, 'NFailedUnits': <uint32 2>}" >/dev/null
"${A11Y[@]}" "SD 2 failed" || fail=1
"${SD[@]}" "$MOCK.UpdateProperties" org.freedesktop.systemd1.Manager \
    "{'SystemState': <'running'>, 'NFailedUnits': <uint32 0>}" >/dev/null
"${A11Y[@]}" "SD ok" || fail=1
vanish org.freedesktop.systemd1
sleep 1
smoke::assert_alive || fail=1
start_systemd
"${SD[@]}" "$MOCK.UpdateProperties" org.freedesktop.systemd1.Manager \
    "{'SystemState': <'degraded'>, 'NFailedUnits': <uint32 1>}" >/dev/null
"${A11Y[@]}" "SD 1 failed" || fail=1
echo "::endgroup::"

echo "::group::wwan"
"${MODEM_SET[@]}" SignalQuality '<(uint32 90, true)>' >/dev/null
"${MODEM_SET[@]}" State '<int32 11>' >/dev/null
"${A11Y[@]}" "WWAN Connected 90% 4G" || fail=1
vanish org.freedesktop.ModemManager1
sleep 1
smoke::assert_alive || fail=1
# The module keeps the first modem it found, so only liveness is checked once MM is back.
start_modemmanager 40
sleep 2
smoke::assert_alive || fail=1
echo "::endgroup::"

echo "::group::inhibitor"
"${CLICK_INHIBITOR[@]}"
"${A11Y[@]}" "INH activated" || fail=1
if ! "${INHIBITORS[@]}" | grep -q "'waybar'"; then
    echo "::error::no waybar inhibitor lock held after the click"; fail=1
fi
"${CLICK_INHIBITOR[@]}"
"${A11Y[@]}" "INH deactivated" || fail=1
sleep 1
if "${INHIBITORS[@]}" | grep -q "'waybar'"; then
    echo "::error::the waybar inhibitor lock is still held after the second click"; fail=1
fi
vanish org.freedesktop.login1
"${CLICK_INHIBITOR[@]}"
sleep 1
smoke::assert_alive || fail=1
"${A11Y[@]}" "INH deactivated" || fail=1
mock org.freedesktop.login1 system -t logind
"${CLICK_INHIBITOR[@]}"
"${A11Y[@]}" "INH activated" || fail=1
echo "::endgroup::"

echo "::group::gamemode"
"${GM[@]}" "$MOCK.UpdateProperties" com.feralinteractive.GameMode "{'ClientCount': <int32 2>}" >/dev/null
"${A11Y[@]}" "GM [2]" || fail=1
vanish com.feralinteractive.GameMode
sleep 1
smoke::assert_alive || fail=1
start_gamemode
"${GM[@]}" "$MOCK.UpdateProperties" com.feralinteractive.GameMode "{'ClientCount': <int32 1>}" >/dev/null
"${A11Y[@]}" "GM [1]" || fail=1
echo "::endgroup::"

echo "::group::mpris"
"${CLICK_MPRIS[@]}"
"${A11Y[@]}" "PAUSED First Song" || fail=1
"${PLAYER_SET[@]}" Metadata "<{'mpris:trackid': <objectpath '/smoke/2'>,
    'xesam:title': <'Second Song'>, 'xesam:artist': <['Smoke Artist']>}>" >/dev/null
"${PLAYER_SET[@]}" PlaybackStatus "<'Playing'>" >/dev/null
"${A11Y[@]}" "PLAYING Smoke Artist - Second Song" || fail=1
vanish org.mpris.MediaPlayer2.smoke
sleep 1
smoke::assert_alive || fail=1
start_player "Third Song"
"${A11Y[@]}" "PLAYING Smoke Artist - Third Song" || fail=1
echo "::endgroup::"

smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1
[ "$fail" = 0 ] || { echo "::error::dbus tier failed"; exit 1; }
echo "dbus tier passed"
