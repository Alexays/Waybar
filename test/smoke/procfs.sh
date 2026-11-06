#!/usr/bin/env bash
# Tier 6 - cpu, memory, cpu_frequency and disk on known inputs. waybar runs
# under bubblewrap with fake /proc/stat, /proc/meminfo, /proc/cpuinfo and
# /sys/devices/system/cpu bind-mounted read-only, and a 16 MiB tmpfs as the
# disk. Every label is asserted to the exact value those inputs give, before and
# after the inputs change. load only gets a render check: glibc's getloadavg()
# calls sysinfo(2), which no bind mount can fake.
#
# Needs unprivileged user namespaces (Ubuntu 24.04 restricts them through
# AppArmor; the workflow lifts that for the job).
#
# Usage: dbus-run-session -- procfs.sh
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$DIR/lib.sh"

export SMOKE_A11Y=1
smoke::setup
smoke::start_compositor
trap '[ $? = 0 ] || { echo "::group::waybar log"; smoke::log; echo "::endgroup::"; }; smoke::stop' EXIT

F="$(mktemp -d)"
mkdir -p "$F/disk" "$F/cpu/cpufreq/policy0" "$F/cpu/cpufreq/policy1"

# Two cores. Between the snapshots cpu0 is 25% busy, cpu1 75%, the total 50%.
# The snapshots have the same length: the bind mount pins the file, so the
# second one is written over the first in place.
cat > "$F/stat" <<'EOF'
cpu  20000 0 20000 160000 0 0 0 0 0 0
cpu0 10000 0 10000 80000 0 0 0 0 0 0
cpu1 10000 0 10000 80000 0 0 0 0 0 0
intr 0
ctxt 0
btime 1700000000
processes 1
EOF
cat > "$F/stat.next" <<'EOF'
cpu  20400 0 20000 160400 0 0 0 0 0 0
cpu0 10100 0 10000 80300 0 0 0 0 0 0
cpu1 10300 0 10000 80100 0 0 0 0 0 0
intr 0
ctxt 0
btime 1700000000
processes 1
EOF

# 16 GiB total, 4 GiB available: 12 GiB (75%) used. Swap half used.
cat > "$F/meminfo" <<'EOF'
MemTotal:       16777216 kB
MemFree:         1048576 kB
MemAvailable:    4194304 kB
SwapTotal:       2097152 kB
SwapFree:        1048576 kB
EOF
sed 's/4194304/8388608/' "$F/meminfo" > "$F/meminfo.next"

# No "cpu MHz" lines, so cpu_frequency reads the cpufreq policies.
printf 'processor\t: 0\n\nprocessor\t: 1\n\n' > "$F/cpuinfo"
for f in present online possible; do echo 0-1 > "$F/cpu/$f"; done
echo 800000 > "$F/cpu/cpufreq/policy0/cpuinfo_min_freq"
echo 3200000 > "$F/cpu/cpufreq/policy0/cpuinfo_max_freq"
echo 1200000 > "$F/cpu/cpufreq/policy1/cpuinfo_min_freq"
echo 2400000 > "$F/cpu/cpufreq/policy1/cpuinfo_max_freq"

# Fills a quarter of the sandbox's tmpfs disk, then becomes waybar.
cat > "$F/fill-disk" <<EOF
#!/bin/sh
head -c 4194304 /dev/zero > "$F/disk/fill" && exec "\$@"
EOF
chmod +x "$F/fill-disk"

cat > "$F/config.json" <<EOF
{ "layer": "top", "position": "top", "height": 30,
  "modules-center": ["cpu", "memory", "load", "cpu_frequency", "disk"],
  "cpu": { "interval": 3, "format": "CPU {usage}% {usage0}/{usage1}" },
  "memory": { "interval": 1,
              "format": "MEM {percentage}% {used:.1f}/{total:.1f} swap {swapPercentage}%" },
  "load": { "format": "LOAD {load1} {load5} {load15}" },
  "cpu_frequency": { "format": "FREQ {min_frequency}/{avg_frequency}/{max_frequency}" },
  "disk": { "path": "$F/disk", "format": "DISK {percentage_used}% of {total}" } }
EOF

bwrap=(bwrap --dev-bind / /
       --ro-bind "$F/stat" /proc/stat
       --ro-bind "$F/meminfo" /proc/meminfo
       --ro-bind "$F/cpuinfo" /proc/cpuinfo
       --ro-bind "$F/cpu" /sys/devices/system/cpu
       --size 16777216 --tmpfs "$F/disk"
       "$F/fill-disk")
bwrap --dev-bind / / true || { echo "::error::bwrap cannot create a sandbox here"; exit 1; }
WAYBAR_WRAP="${bwrap[*]}" smoke::launch_waybar "$F/config.json" "$DIR/style.css" 1
smoke::assert_alive

A11Y=(python3 "$DIR/a11y.py" --timeout 8)

echo "::group::initial values"
"${A11Y[@]}" "CPU 0% 0/0" "MEM 75% 12.0/16.0 swap 50%" "LOAD " "FREQ 0.8/1.9/3.2" "DISK 25% of 16.0MiB"
echo "::endgroup::"

echo "::group::inputs change"
for f in stat meminfo; do
    dd if="$F/$f.next" of="$F/$f" bs=4096 conv=notrunc status=none
done
"${A11Y[@]}" "CPU 50% 25/75" "MEM 50% 8.0/16.0 swap 50%"
echo "::endgroup::"

# bwrap does not forward signals: interrupt waybar itself, then let
# assert_clean_exit reap bwrap, which exits with waybar's status.
kill -INT "$(pgrep -P "$WAYBAR_PID")"
smoke::assert_clean_exit
echo "✓ procfs tier passed"
