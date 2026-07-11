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

[ "$fail" = 0 ] || { echo "::error::lifecycle tier failed"; exit 1; }
echo "✓ lifecycle tier passed"
