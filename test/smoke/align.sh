#!/usr/bin/env bash
# Tier 3 — label alignment: a custom module wider than its text must place the
# text according to `align` / `justify` instead of pinning it to the left edge.
# Measures the rendered text's bounding box in the screenshot.
#
# Usage: align.sh [screenshot-dir]
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

SHOTDIR="${1:-align-shots}"
mkdir -p "$SHOTDIR"

# The module is centered on the bar and MODULE_W px wide, so it spans
# [MOD_L, MOD_R] on the 1920 px wide output.
MODULE_W=400
MOD_L=$(( (WIDTH - MODULE_W) / 2 ))
MOD_R=$(( MOD_L + MODULE_W ))
MID=$(( WIDTH / 2 ))

style="$(mktemp --suffix=.css)"
cat > "$style" <<EOF
* { font-family: "DejaVu Sans"; font-size: 14px; border: none; border-radius: 0; min-height: 0; }
window#waybar { background-color: #000000; color: #ffffff; }
#custom-a { min-width: ${MODULE_W}px; padding: 0; margin: 0; background-color: #000000; color: #ffffff; }
EOF

smoke::setup
trap smoke::stop EXIT

fails=()

# run_case <name> <extra module options> <check>
# <check> is an awk condition over x0 (left edge), x1 (right edge), cx (center)
# of the text's bounding box.
run_case() {
    local name="$1" opts="$2" check="$3"
    echo "::group::align: $name ($opts)"
    smoke::start_compositor || { fails+=("$name"); echo "::endgroup::"; return; }
    local cfg; cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["custom/a"],
  "custom/a": { "exec": "echo XX", "interval": "once"${opts:+, $opts} }
}
EOF
    smoke::launch_waybar "$cfg" "$style" 4
    local ok=1
    smoke::assert_alive || ok=0
    smoke::assert_clean || ok=0
    smoke::screenshot "$SHOTDIR/$name.png" || ok=0
    if [ "$ok" = 1 ]; then
        # Bounding box of the non-black (text) pixels in the bar strip.
        local box
        box="$(convert "$SHOTDIR/$name.png" -crop "${WIDTH}x30+0+0" +repage \
                 -colorspace Gray -threshold 50% -format '%@' info:)"
        local w x
        w="${box%%x*}"
        x="${box#*+}"; x="${x%%+*}"
        local x0="$x" x1=$(( x + w ))
        local cx=$(( (x0 + x1) / 2 ))
        echo "text box $box: x0=$x0 x1=$x1 cx=$cx (module $MOD_L..$MOD_R, mid $MID)"
        if awk -v x0="$x0" -v x1="$x1" -v cx="$cx" -v l="$MOD_L" -v r="$MOD_R" -v mid="$MID" \
               "BEGIN { exit !($check) }"; then
            echo "✓ $name: text placed as expected"
        else
            echo "::error::$name: text box x0=$x0 x1=$x1 cx=$cx fails: $check"
            ok=0
        fi
    fi
    [ "$ok" = 1 ] || fails+=("$name")
    smoke::stop
    echo "::endgroup::"
}

run_case default         ''                    'cx >= mid - 10 && cx <= mid + 10'
run_case justify-center  '"justify": "center"' 'cx >= mid - 10 && cx <= mid + 10'
run_case align-left      '"align": 0.0'        'x0 <= l + 10'
run_case align-right     '"align": 1.0'        'x1 >= r - 10'

if [ "${#fails[@]}" -gt 0 ]; then
    echo "::error::alignment cases failed: ${fails[*]}"
    exit 1
fi
echo "✓ all alignment cases placed the text as configured"
