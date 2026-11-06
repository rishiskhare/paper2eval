#!/usr/bin/env python3
"""PGM verifier. Code-only, deterministic.

Gates: allowed files -> clean rebuild -> trusted correctness -> bottom-level segments vs reference
(the paper's optimal PLA-model; totals over all levels are reported, not gated). Build time is recorded, not gated.
The submission's process runs as an unprivileged user (pgm_check._sandbox) and can only write its own dump.
Writes results.json (details), reward.json ({reward, pass}; reward = pass) and reward.txt (pass).
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pgm_check  # noqa: E402

# The trusted index may add one segment depending on where the last line lands (pgm_index.hpp), so an optimal
# segmentation can differ from the reference by one. The non-optimal methods miss by far more.
SEGMENT_TOLERANCE = 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--submission", required=True, help="agent's repo tree")
    ap.add_argument("--pristine", required=True, help="fresh stripped repo tree")
    ap.add_argument("--harness", required=True)
    ap.add_argument("--spec", required=True, help="held-out workload spec")
    ap.add_argument("--gen", default=str(Path(__file__).resolve().parent / "gen_workloads.py"))
    ap.add_argument("--reference", help="reference_values.json")
    ap.add_argument("--record-reference", help="write measured values here instead of scoring")
    ap.add_argument("--workdir", required=True)
    ap.add_argument("--out-dir", required=True)
    a = ap.parse_args()

    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    work = Path(a.workdir).resolve()
    res = {"task": "pgm", "metric": "bottom-level segments (= segments_per_level[0]), must be within SEGMENT_TOLERANCE of the optimal reference",
           "gates": {}, "gate_failed": None, "reason": None, "workloads": {}}

    def finish(passed=0):
        res.update({"pass": passed, "reward": passed})
        (out / "results.json").write_text(json.dumps(res, indent=2) + "\n")
        (out / "reward.json").write_text(json.dumps({"reward": passed, "pass": passed}) + "\n")
        (out / "reward.txt").write_text(f"{passed}\n")
        print(json.dumps({k: res[k] for k in ("gate_failed", "reason", "pass", "reward")}))
        return 0

    def gate(name, info, reason=None):
        res["gates"][name] = info
        if not info["ok"]:
            res["gate_failed"], res["reason"] = name, reason or json.dumps(info)[:2000]
            return False
        return True

    g = pgm_check.check_allowed_files(a.submission, a.pristine)
    if not gate("allowed_files", g, f"files outside the allowed set changed: {g['violations']}"):
        return finish()
    g = pgm_check.rebuild(a.submission, a.pristine, a.harness, work / "build")
    if not gate("clean_rebuild", g, " | ".join(g["errors"])):
        return finish()

    wl_dir = work / "workloads"
    subprocess.run([sys.executable, a.gen, a.spec, str(wl_dir)], check=True, capture_output=True)
    specs = json.loads(Path(a.spec).read_text())["workloads"]
    ref = json.loads(Path(a.reference).read_text()) if a.reference else None

    measured, failures = {}, []
    for s in specs:
        path = wl_dir / f"{s['name']}.bin"
        sha = hashlib.sha256(path.read_bytes()).hexdigest()
        if ref and ref["workloads"][s["name"]]["sha256"] != sha:
            res["gate_failed"], res["reason"] = "infrastructure", f"{s['name']}: workload bytes differ from reference"
            return finish()
        r = pgm_check.run_workload(work / "build", path)
        entry = {"sha256": sha, "correct": r["correct"]}
        if r["correct"]:
            entry["eps"] = {str(x["eps"]): {"total_segments": x["total_segments"],
                                            "segments_per_level": x["segments_per_level"],
                                            "build_ms": x["build_ms"]} for x in r["results"]}
        else:
            entry["reason"] = r["reason"]
            failures.append(f"{s['name']}: {r['reason']}")
        measured[s["name"]] = entry
    res["workloads"] = measured
    res["gates"]["correctness"] = {"ok": not failures, "failures": failures}
    if failures:
        res["gate_failed"], res["reason"] = "correctness", " | ".join(failures)[:4000]
        return finish()

    if a.record_reference:
        rec = {"workloads": {n: {"sha256": m["sha256"],
                                 "eps": {e: {"bottom_segments": v["segments_per_level"][0], "total_segments": v["total_segments"]}
                                         for e, v in m["eps"].items()}}
                             for n, m in measured.items()}}
        Path(a.record_reference).write_text(json.dumps(rec, indent=2) + "\n")
        return finish(1)

    worse, checked = [], 0
    for n, m in measured.items():
        for e, v in m["eps"].items():
            r = ref["workloads"][n]["eps"][e]
            v.update({"bottom_segments": v["segments_per_level"][0], "reference_bottom_segments": r["bottom_segments"],
                      "reference_total_segments": r["total_segments"], "threshold": f"bottom <= {r['bottom_segments'] + SEGMENT_TOLERANCE}"})
            checked += 1
            if v["bottom_segments"] > r["bottom_segments"] + SEGMENT_TOLERANCE:
                worse.append(f"{n}/eps={e} ({v['bottom_segments']} > {r['bottom_segments']})")
    res["gates"]["metric"] = {"ok": not worse, "worse_than_reference": worse}
    if worse:
        res["gate_failed"] = "metric"
        res["reason"] = f"more bottom-level segments than the optimal reference on {len(worse)} of {checked}: {worse[:10]}"
        return finish()
    return finish(1)


if __name__ == "__main__":
    sys.exit(main())
