#!/bin/zsh
# fps.sh <outdir> [seconds]: samples SurfaceFlinger present timestamps of the running Cemu game layer
# (PKG, default info.cemu.cemu.odin3) once a second, then prints host fps and slow frames.
# Never screencap during a run: each capture stalls SurfaceFlinger.
A=~/Library/Android/sdk/platform-tools/adb
OUT=$1; DUR=${2:-60}; PKG=${PKG:-info.cemu.cemu.odin3}
mkdir -p $OUT
L=$($A shell "dumpsys SurfaceFlinger --list" | tr -d '\r' | grep -oE "SurfaceView\[$PKG/[^]]*EmulationActivity\]\(BLAST\)#[0-9]+" | head -1)
[ -z "$L" ] && { echo "no Cemu layer for $PKG"; exit 1; }
echo "layer=$L start=$(date +%T)" > $OUT/context.txt
: > $OUT/sf_latency.txt
t0=$SECONDS
while [ $((SECONDS - t0)) -lt $DUR ]; do
  # SF keeps only the last 128 presents (~2.1 s at 60 fps), so sample faster than that.
  $A shell "dumpsys SurfaceFlinger --latency '$L'" >> $OUT/sf_latency.txt
  sleep 1
done
python3 -I ${0:A:h}/fps.py $OUT/sf_latency.txt | tee $OUT/summary.txt
