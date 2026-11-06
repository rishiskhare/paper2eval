#!/usr/bin/env python3
"""Builds the repo's segmentation against an untouched copy of the code, checks the results with the
untouched code, and prints the numbers next to the upstream implementation's."""
import argparse
import json
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pgm_check as h  # noqa: E402

ap = argparse.ArgumentParser()
for x in ["--repo", "--upstream", "--harness", "--data", "--counts"]:
    ap.add_argument(x, required=True)
a = ap.parse_args()
g = h.check_allowed_files(a.repo, a.upstream)
if not g["ok"]:
    print(f"note: files other than {h.ALLOWED} differ from upstream: {g['violations']}")
work = Path(tempfile.mkdtemp(prefix="pgm-check-"))
g = h.rebuild(a.repo, a.upstream, a.harness, work)
if not g["ok"]:
    sys.exit("build failed:\n" + "\n".join(g["errors"]))
up = json.loads(Path(a.counts).read_text())
ok = True
print(f"{'keys':<16}{'eps':>5}{'segments':>10}{'upstream':>10}   all levels")
for name in sorted(up["workloads"]):
    r = h.run_workload(work, Path(a.data, name + ".bin"))
    if not r["correct"]:
        ok = False
        print(f"{name}: incorrect: {r['reason']}")
        continue
    for x in r["results"]:
        u = up["workloads"][name]["eps"][str(x["eps"])]
        print(f"{name:<16}{x['eps']:>5}{x['segments_per_level'][0]:>10}{u['bottom_segments']:>10}   "
              f"{x['total_segments']} (upstream {u['total_segments']})")
print("\nall lookups correct" if ok else "\nsome lookups incorrect")
sys.exit(0 if ok else 1)
