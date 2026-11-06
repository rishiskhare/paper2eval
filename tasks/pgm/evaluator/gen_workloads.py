#!/usr/bin/env python3
"""Deterministic PGM workloads: sorted, unique little-endian uint64 keys, written as <name>.bin.

Pure-Python `random.Random(seed)` so the bytes do not depend on numpy versions.
Usage: gen_workloads.py <spec.json> <out_dir>
"""
import hashlib
import json
import math
import random
import struct
import sys
from pathlib import Path

MAX_KEY = 2**64 - 2  # 2**64 - 1 is PGMIndex's sentinel


def _finish(keys, n):
    keys = sorted({k for k in keys if 0 <= k <= MAX_KEY})
    return keys[:n] if len(keys) > n else keys


def uniform(r, n, bits=64, lo=0):
    return _finish((lo + r.getrandbits(bits) for _ in range(int(n * 1.01))), n)


def lognormal(r, n, mu=0.0, sigma=2.0, scale=1e12):
    return _finish((int(r.lognormvariate(mu, sigma) * scale) for _ in range(int(n * 1.05))), n)


def normal(r, n, mu=2**62, sigma=2**55):
    return _finish((int(r.gauss(mu, sigma)) for _ in range(int(n * 1.01))), n)


def clustered(r, n, clusters=200, spread=2**30, span=2**60):
    centers = [r.getrandbits(60) % span for _ in range(clusters)]
    return _finish((int(r.gauss(r.choice(centers), spread)) for _ in range(int(n * 1.02))), n)


def gaps(r, n, alpha=1.3, start=0):
    """Cumulative Pareto gaps: heavy-tailed spacing like real ID columns."""
    out, k = [], start
    for _ in range(n):
        k += 1 + int(r.paretovariate(alpha))
        out.append(k)
    return _finish(out, n)


def near_max(r, n, width=2**40):
    return _finish((MAX_KEY - r.getrandbits(int(math.log2(width))) for _ in range(int(n * 1.01))), n)


def dense_runs(r, n, mean_run=500, max_gap=2**20):
    out, k = [], r.getrandbits(40)
    while len(out) < n:
        run = 1 + int(r.expovariate(1 / mean_run))
        out.extend(range(k, k + run))
        k += run + 1 + r.randrange(max_gap)
    return _finish(out, n)


def long_gaps(r, n, groups=50, gap_bits=50):
    out, k, per = [], 0, n // groups
    for _ in range(groups):
        k += 2**gap_bits + r.getrandbits(gap_bits)
        step = 1 + r.randrange(1000)
        out.extend(k + i * step + r.randrange(step) for i in range(per))
        k = out[-1]
    return _finish(out, n)


def mixed(r, n):
    parts = [uniform(r, n // 4, 40), gaps(r, n // 4, 1.1, 2**41), dense_runs(r, n // 4),
             near_max(r, n // 4)]
    return _finish([k for p in parts for k in p], n)


KINDS = {f.__name__: f for f in (uniform, lognormal, normal, clustered, gaps, near_max, dense_runs, long_gaps, mixed)}


def generate(spec):
    r = random.Random(spec["seed"])
    keys = KINDS[spec["kind"]](r, spec["n"], **spec.get("params", {}))
    return struct.pack("<%dQ" % len(keys), *keys)


def main():
    spec_path, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    for spec in json.loads(spec_path.read_text())["workloads"]:
        data = generate(spec)
        (out_dir / f"{spec['name']}.bin").write_bytes(data)
        print(f"{spec['name']}: {len(data) // 8} keys sha256={hashlib.sha256(data).hexdigest()[:16]}")


if __name__ == "__main__":
    main()
