#!/usr/bin/env bash
# Tier 1 — per-module matrix: render each headless-safe module in isolation and
# assert it loads, logs no sanitizer/critical error, and draws something.
#
# Usage: modules.sh <screenshot-dir>
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

SHOTDIR="${1:-module-shots}"
mkdir -p "$SHOTDIR"

# module name -> its config options (JSON object). Only modules that work with
# no real hardware/daemon in a headless runner are included.
declare -A MODULES=(
    [clock]='{}'
    [cpu]='{}'
    [memory]='{}'
    [disk]='{}'
    [idle_inhibitor]='{"format":"{status}"}'
    [custom/echo]='{"exec":"echo hello","interval":"once"}'
)

smoke::setup
smoke::start_compositor
trap smoke::stop EXIT

fails=()
for mod in "${!MODULES[@]}"; do
    safe="${mod//\//-}"
    cfg="$(mktemp --suffix=.json)"
    cat > "$cfg" <<EOF
{
  "layer": "top", "position": "top", "height": 30,
  "modules-center": ["$mod"],
  "$mod": ${MODULES[$mod]}
}
EOF
    echo "::group::module: $mod"
    smoke::launch_waybar "$cfg" "$DIR/style.css" 4
    ok=1
    smoke::assert_alive || ok=0
    smoke::assert_clean || ok=0
    if [ "$ok" = 1 ]; then
        smoke::screenshot "$SHOTDIR/$safe.png"
        smoke::assert_not_blank "$SHOTDIR/$safe.png" || ok=0
    fi
    [ "$ok" = 1 ] || fails+=("$mod")
    # SIGINT teardown (not SIGTERM) so each module's destructor path is ASan-checked.
    smoke::assert_clean_exit || fails+=("$mod (exit)")
    echo "::endgroup::"
done

# --- all modules inside a group -----------------------------------------
# Group children have a different construct/destroy path than top-level modules
# (#5179 crashed inside a group), so render the whole matrix wrapped in one.
echo "::group::modules inside a group"
mods_json="$(printf '%s\n' "${!MODULES[@]}" | jq -R . | jq -s .)"
opts_json='{}'
for mod in "${!MODULES[@]}"; do
    opts_json="$(jq --arg k "$mod" --argjson v "${MODULES[$mod]}" '. + {($k):$v}' <<<"$opts_json")"
done
gcfg="$(mktemp --suffix=.json)"
jq -n --argjson mods "$mods_json" --argjson opts "$opts_json" '
  {layer:"top",position:"top",height:30,"modules-center":["group/g"],"group/g":{modules:$mods}} + $opts' \
  > "$gcfg"
smoke::launch_waybar "$gcfg" "$DIR/style.css" 5
ok=1
smoke::assert_alive || ok=0
smoke::assert_clean || ok=0
[ "$ok" = 1 ] && { smoke::screenshot "$SHOTDIR/_group.png"; smoke::assert_not_blank "$SHOTDIR/_group.png" || ok=0; }
smoke::assert_clean_exit || ok=0
[ "$ok" = 1 ] || fails+=("group")
echo "::endgroup::"

if [ "${#fails[@]}" -gt 0 ]; then
    echo "::error::modules failed to render cleanly: ${fails[*]}"
    exit 1
fi
echo "✓ all ${#MODULES[@]} modules rendered cleanly (standalone + grouped)"
