"""Build and run helpers for the pgm segmentation/compression tools: allowed-file diff, clean rebuild, trusted run."""
import json
import os
import pwd
import shutil
import subprocess
from pathlib import Path

ALLOWED = ["include/pgm/piecewise_linear_model.hpp"]
CXX = ["g++", "-std=c++17", "-O2", "-w"]
EPS = [32, 64, 128]
COMPILE_TIMEOUT = 300
BUILD_TIMEOUT = 300
CHECK_TIMEOUT = 300
SANDBOX_USER = "grader"  # unprivileged user for the submission's process when the grader runs as root
IGNORED_DIRS = {".git", "build", "cmake-build-debug", "cmake-build-release", "__pycache__"}


def tree_files(root):
    root = Path(root)
    out = {}
    for p in sorted(root.rglob("*")):
        rel = p.relative_to(root)
        if any(part in IGNORED_DIRS for part in rel.parts):
            continue
        if p.is_symlink():
            out[str(rel)] = b"symlink:" + str(p.readlink()).encode()
        elif p.is_file():
            out[str(rel)] = p.read_bytes()
    return out


def check_allowed_files(submission, pristine):
    sub, ref = tree_files(submission), tree_files(pristine)
    # New untracked files (scratch tests, build dirs) are ignored: grading rebuilds from pristine + ALLOWED only.
    changed = sorted(f for f in ref if sub.get(f) != ref.get(f))
    bad = [f for f in changed if f not in ALLOWED]
    for f in ALLOWED:
        if f in sub and Path(submission, f).is_symlink():
            bad.append(f + " (symlink)")
    return {"changed": changed, "added": sorted(set(sub) - set(ref)), "violations": bad, "ok": not bad}


def _run(cmd, timeout, **kw):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, **kw)
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return None, "", f"timeout after {timeout}s"


def rebuild(submission, pristine, harness, workdir):
    """Fresh copy of the stripped tree + only the allowed files from the submission, then compile both binaries."""
    workdir = Path(workdir).resolve()
    if workdir.exists():
        shutil.rmtree(workdir)
    tree = workdir / "tree"
    shutil.copytree(pristine, tree, ignore=shutil.ignore_patterns(*IGNORED_DIRS))
    for f in ALLOWED:
        src = Path(submission, f)
        if src.is_file():
            shutil.copyfile(src, tree / f)
        elif (tree / f).exists():
            (tree / f).unlink()
    out = {"ok": True, "errors": []}
    for name, defn, inc, extra in [("harness_build", "HARNESS_BUILD", tree, []),
                                   ("harness_check", "HARNESS_CHECK", Path(pristine), ["-D_GLIBCXX_ASSERTIONS"])]:
        code, _, err = _run(CXX + extra + [f"-D{defn}", "-I", str(inc / "include"), str(harness),
                                           "-o", str(workdir / name)], COMPILE_TIMEOUT)
        if code != 0:
            out["ok"] = False
            out["errors"].append(f"{name}: compile failed: {err.strip()[-2000:]}")
    return out


def _sandbox(box):
    """As root: run the submission's process as SANDBOX_USER, writing only to `box`. Otherwise run as-is."""
    if os.geteuid() != 0:
        return []
    try:
        u = pwd.getpwnam(SANDBOX_USER)
    except KeyError:
        return []
    os.chown(box, u.pw_uid, u.pw_gid)
    return ["setpriv", f"--reuid={u.pw_uid}", f"--regid={u.pw_gid}", "--clear-groups", "--"]


def run_workload(workdir, keys_path):
    workdir = Path(workdir).resolve()
    keys_path = Path(keys_path).resolve()
    box = workdir / "box"
    if box.exists():
        shutil.rmtree(box)
    box.mkdir(mode=0o700)
    dump = box / (keys_path.stem + ".dump")
    code, _, err = _run(_sandbox(box) + [str(workdir / "harness_build"), str(keys_path), str(dump)], BUILD_TIMEOUT, cwd=box)
    if code != 0:
        return {"correct": False, "reason": f"index build failed (exit {code}): {err.strip()[-500:]}"}
    code, out, err = _run([str(workdir / "harness_check"), str(keys_path), str(dump)], CHECK_TIMEOUT)
    dump.unlink(missing_ok=True)
    try:
        res = json.loads(out)
    except json.JSONDecodeError:
        return {"correct": False, "reason": f"checker crashed (exit {code}): {err.strip()[-500:]}"}
    if not res["correct"]:
        bad = [r for r in res["results"] if not r["correct"]]
        res["reason"] = "; ".join(f"eps={r['eps']}: {r['reason']}" for r in bad) or "trailing bytes in dump"
    return res
