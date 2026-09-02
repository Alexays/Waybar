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

[ "$fail" = 0 ] || { echo "::error::fuzz tier failed"; exit 1; }
echo "✓ custom-backend fuzz passed"
