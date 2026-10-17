# fps.py <sf_latency.txt>: host fps and slow frames from merged `dumpsys SurfaceFlinger --latency` samples.
import sys

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
print(f"frames={len(gaps)} seconds={dur:.1f} fps={len(gaps) / dur:.1f} "
      f"slow(>25ms)={100 * len(slow) / len(gaps):.1f}% worst={max(gaps):.1f}ms")
