#!/bin/zsh
# fps.sh <outdir> [seconds]: samples SurfaceFlinger present timestamps of the running Cemu game layer
# (PKG, default info.cemu.cemu.odin3) once a second, then prints host fps, slow frames and pacing, the average
# GPU busy/clock, the CPU load of the busiest Cemu threads and how often the emulation threads ran on the prime cores.
# Never screencap during a run: each capture stalls SurfaceFlinger.
A=~/Library/Android/sdk/platform-tools/adb
OUT=$1; DUR=${2:-60}; PKG=${PKG:-info.cemu.cemu.odin3}
K=/sys/class/kgsl/kgsl-3d0
mkdir -p $OUT
L=$($A shell "dumpsys SurfaceFlinger --list" | tr -d '\r' | grep -oE "SurfaceView\[$PKG/[^]]*EmulationActivity\]\(BLAST\)#[0-9]+" | head -1)
[ -z "$L" ] && { echo "no Cemu layer for $PKG"; exit 1; }
# The emulation runs in its own ":EmulationProcess" on SSimco's port.
P=$($A shell pidof $PKG:EmulationProcess || $A shell pidof $PKG); P=${P//$'\r'/}
[ -z "$P" ] && { echo "no Cemu process for $PKG"; exit 1; }
echo "layer=$L pid=$P start=$(date +%T)" > $OUT/context.txt
threads() { $A shell "cd /proc/$P/task && for t in *; do echo \$t \$(cat \$t/comm | tr ' ' _) \$(sed 's/.*) //' \$t/stat | cut -d' ' -f12,13); done"; }
threads > $OUT/threads0.txt
: > $OUT/sf_latency.txt; : > $OUT/gpu.txt
t0=$SECONDS
while [ $((SECONDS - t0)) -lt $DUR ]; do
  # SF keeps only the last 128 presents (~2.1 s at 60 fps), so sample faster than that.
  $A shell "dumpsys SurfaceFlinger --latency '$L'; echo GPU \$(cat $K/gpu_busy_percentage) \$(cat $K/gpuclk) >&2;
    for t in /proc/$P/task/*; do read c < \$t/comm; case \$c in OSSched*|LatteThread) echo PSR \$c \$(sed 's/.*) //' \$t/stat | cut -d' ' -f37) >&2;; esac; done" >> $OUT/sf_latency.txt 2>> $OUT/gpu.txt
  sleep 1
done
threads > $OUT/threads1.txt
python3 -I ${0:A:h}/fps.py $OUT/sf_latency.txt $OUT/gpu.txt $OUT/threads0.txt $OUT/threads1.txt $((SECONDS - t0)) | tee $OUT/summary.txt
