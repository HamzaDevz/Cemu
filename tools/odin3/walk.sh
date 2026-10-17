# walk.sh <seconds>: runs on the device; walks player 1 (pad.sh) right then left with jumps, along with the
# virtual pads of vpads.sh when they are running.
T=/data/local/tmp
pads() { for i in 1 2 3; do [ -p $T/vp$i ] && pgrep -f "vpad$i" >/dev/null && echo "$1" > $T/vp$i; done; }
end=$(( $(date +%s) + ${1:-60} ))
while [ $(date +%s) -lt $end ]; do
  for x in 32767 -32767; do
    v=0; [ $x -gt 0 ] && v=255
    pads "abs lx $v"
    sh $T/pad.sh stick $x 0 1800
    pads "tap a 150"
    sh $T/pad.sh btn b 300
  done
done
pads "abs lx 128"
