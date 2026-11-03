"""Compare two f32 interleaved captures produced by tools/research/bench --null.

Usage:
    python3 tools/research/nullcmp.py before.f32 after.f32

A plumbing-only change must report exactly 0.0 (bit-identical). A numeric change
is accepted at or below -120 dBFS peak.
"""
import array
import math
import os
import sys


def load(path):
    a = array.array("f")
    with open(path, "rb") as f:
        a.fromfile(f, os.path.getsize(path) // 4)
    return a


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2

    a, b = load(argv[1]), load(argv[2])
    if len(a) != len(b):
        print(f"LENGTH MISMATCH: {len(a)} vs {len(b)} floats")
        return 1

    n = len(a)
    peak = 0.0
    peak_i = -1
    sq = 0.0
    for i in range(n):
        d = a[i] - b[i]
        ad = abs(d)
        if ad > peak:
            peak, peak_i = ad, i
        sq += d * d

    rms = math.sqrt(sq / n) if n else 0.0
    print(f"samples        : {n} ({n // 2} frames)")
    print(f"peak abs diff  : {peak:.9e}  (sample {peak_i})")
    print(f"rms  abs diff  : {rms:.9e}")
    if peak > 0.0:
        print(f"peak diff dBFS : {20.0 * math.log10(peak):.1f}")

    if peak == 0.0:
        print("VERDICT: BIT-IDENTICAL")
        return 0
    if 20.0 * math.log10(peak) <= -120.0:
        print("VERDICT: PASS (<= -120 dBFS)")
        return 0
    print("VERDICT: FAIL (> -120 dBFS)")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
