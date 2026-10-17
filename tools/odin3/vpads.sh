# vpads.sh [stop]: runs on the device; creates three virtual DS4 pads (vpad, built from vpad.c with the NDK clang
# and pushed to /data/local/tmp) fed by the FIFOs /data/local/tmp/vp1..vp3, e.g. echo "tap a" > /data/local/tmp/vp2.
T=/data/local/tmp
pkill vpad
for p in $(ps -A -o PID,ARGS | grep "do cat $T/vp" | grep -v grep | while read pid rest; do echo $pid; done); do kill $p; done
[ "$1" = stop ] && exit
for i in 1 2 3; do
  [ -p $T/vp$i ] || mkfifo $T/vp$i
  nohup sh -c "while true; do cat $T/vp$i; done | $T/vpad 'Wireless Controller' 054c 09cc vpad$i" >/dev/null 2>&1 &
done
