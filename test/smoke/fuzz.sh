#!/usr/bin/env bash
# Tier — custom-backend fuzz. The custom module's exec output flows straight into
# formatting / sanitising / (optionally) JSON parsing, so feed it the pathological
# inputs that tend to segfault (empty, non-zero exit, invalid JSON, huge, non-UTF8,
# empty format) and assert no crash / critical, including on clean exit.
#
# Usage: fuzz.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

fail=0

# run_case <name> <custom-module-json>
run_case() {
    local name="$1" mod="$2" cfg
    echo "::group::fuzz: $name"
    cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<EOF
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom/x"], "custom/x": $mod }
EOF
    smoke::launch_waybar "$cfg" "$DIR/style.css" 4
    smoke::assert_alive || fail=1
    smoke::assert_clean_exit || fail=1
    echo "::endgroup::"
}

run_case "empty output"      '{"exec":"true","interval":"once"}'
run_case "nonzero exit"      '{"exec":"echo hi; exit 3","interval":"once"}'
run_case "invalid json"      '{"exec":"echo not-json","return-type":"json","interval":"once"}'
run_case "huge output"       '{"exec":"yes A | head -c 100000","interval":"once"}'
run_case "non-utf8 bytes"    '{"exec":"printf \\377\\376\\375","interval":"once"}'
run_case "empty format"      '{"exec":"echo x","interval":"once","format":""}'
run_case "newlines flood"    '{"exec":"yes | head -n 500","interval":"once"}'
run_case "non-utf8 class"    '{"exec":"printf '\''{\"text\":\"x\",\"class\":\"\\351\"}\\n'\''","return-type":"json","interval":1}'
# An embedded icon directive naming a device must be refused, not read on the main loop
run_case "embedded icon device" '{"exec":"printf %s \\\\0icon\\\\1f/dev/ptmx\\\\n","interval":"once"}'
# Wider or taller than the 32767 pixels a GDK surface can have
run_case "long line"         '{"exec":"printf %05000d 0","interval":"once"}'
run_case "long rotated line" '{"exec":"printf %05000d 0","interval":"once","rotate":90}'

# image module: the path printed by exec is data and must never be shell-expanded.
echo "::group::fuzz: image exec output with command substitution"
marker="$(mktemp -u)"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<EOF
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["image"],
  "image": {"exec":"echo '/tmp/\$(touch $marker)cover.png'","interval":"once"} }
EOF
smoke::launch_waybar "$cfg" "$DIR/style.css" 4
smoke::assert_alive || fail=1
if [ -e "$marker" ]; then
    echo "::error::image module ran a command embedded in its exec output"
    rm -f "$marker"
    fail=1
else
    echo "image exec output was not shell-expanded"
fi
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

# image module: an invalid UTF-8 tooltip line (the 2nd) must not crash once the image loads
echo "::group::fuzz: image non-utf8 tooltip"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<EOF
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["image"],
  "image": {"exec":"printf '$DIR/reference.png\\\\nCaf\\\\351\\\\n'","interval":"once"} }
EOF
smoke::launch_waybar "$cfg" "$DIR/style.css" 4
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1
echo "::endgroup::"

# image module: a FIFO at the path must be refused, not opened on any thread
echo "::group::fuzz: image path is a fifo"
fifo="$(mktemp -u)"
mkfifo "$fifo"
cfg="$(mktemp --suffix=.json)"
cat > "$cfg" <<EOF
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["image"], "image": {"path":"$fifo","interval":1} }
EOF
smoke::launch_waybar "$cfg" "$DIR/style.css" 4
smoke::assert_alive || fail=1
smoke::assert_clean_exit || fail=1
rm -f "$fifo"
echo "::endgroup::"

[ "$fail" = 0 ] || { echo "::error::fuzz tier failed"; exit 1; }
echo "✓ custom-backend fuzz passed"
