# fps.py <sf_latency.txt> [gpu.txt threads0.txt threads1.txt seconds]: host fps, slow frames and pacing from merged
# `dumpsys SurfaceFlinger --latency` samples, plus GPU busy/clock and per-thread CPU load when fps.sh gathered them.
import sys
from collections import defaultdict

presents = set()
for line in open(sys.argv[1]):
    parts = line.split()
    # Rows are "desired actual ready" in ns; pending frames carry INT64_MAX.
    if len(parts) == 3 and parts[1].isdigit() and 0 < int(parts[1]) < 2**62:
        presents.add(int(parts[1]))
t = sorted(presents)
if len(t) < 2:
    sys.exit("not enough frames")
gaps = [(b - a) / 1e6 for a, b in zip(t, t[1:])]
# Drop holes from sampling gaps (more than 128 frames between two dumps would show as one huge gap).
gaps = [g for g in gaps if g < 500]
dur = sum(gaps) / 1000
slow = [g for g in gaps if g > 16.67 * 1.5]
# Pacing: at 60 fps every frame should stay 16.7 ms on screen; on the 120 Hz panel a frame held 8.3 or 25 ms is judder.
offpace = [g for g in gaps if not 12.5 <= g <= 20.8]
s = sorted(gaps)
p99 = s[min(len(s) - 1, int(len(s) * 0.99))]
mean = dur * 1000 / len(gaps)
sd = (sum((g - mean) ** 2 for g in gaps) / len(gaps)) ** 0.5
print(f"frames={len(gaps)} seconds={dur:.1f} fps={len(gaps) / dur:.1f} "
      f"slow(>25ms)={100 * len(slow) / len(gaps):.1f}% worst={max(gaps):.1f}ms "
      f"offpace={100 * len(offpace) / len(gaps):.1f}% p99={p99:.1f}ms sd={sd:.2f}ms")

if len(sys.argv) < 6:
    sys.exit()
busy, clk = [], []
psr = defaultdict(list)
for line in open(sys.argv[2]):
    parts = line.replace('%', '').split()
    if len(parts) == 3 and parts[0] == 'GPU' and parts[1].isdigit():
        busy.append(int(parts[1])); clk.append(int(parts[2]) / 1e6)
    elif len(parts) == 3 and parts[0] == 'PSR' and parts[2].isdigit():
        psr[parts[1]].append(int(parts[2]))
if busy:
    print(f"gpu_busy={sum(busy) / len(busy):.0f}% gpu_mhz={sum(clk) / len(clk):.0f}")
# cpu6-7 are the 4.32 GHz prime cores, cpu0-5 run at 3.53 GHz.
if psr:
    print("on_prime " + " ".join(f"{n}={100 * sum(c >= 6 for c in v) / len(v):.0f}%" for n, v in sorted(psr.items())))


def ticks(path):
    r = {}
    for line in open(path):
        parts = line.split()
        if len(parts) == 4 and parts[2].isdigit():
            r[parts[0]] = (parts[1], int(parts[2]) + int(parts[3]))
    return r


t0, t1, secs = ticks(sys.argv[3]), ticks(sys.argv[4]), float(sys.argv[5])
# USER_HZ is 100 on Android: ticks / seconds = % of one core.
load = defaultdict(float)
for tid, (name, v) in t1.items():
    load[name] += (v - t0.get(tid, (name, 0))[1]) / secs
total = sum(load.values())
top = sorted(load.items(), key=lambda kv: -kv[1])[:6]
print(f"cpu_total={total:.0f}% " + " ".join(f"{n}={v:.0f}%" for n, v in top))
