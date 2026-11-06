# pgm: known limits

- **The paper is given, and it describes the algorithm in prose only.** `~/papers/` (`pgm-index.pdf` + `.txt`) holds the VLDB 2020 paper (PDF
  and extracted text). Section 2.1 explains the convex-hull idea and cites O'Rourke for the optimal streaming
  algorithm; turning that into code inside this repo is the task.
- **Agents may know the method.** The paper (2020) and its code are public and likely in training data.
- **Gate is the bottom level.** Pass = correct, and no more bottom-level segments than the authors' code on every
  held-out (workload, ε). The paper claims this level is optimal, so the count is fixed by the paper. Upper levels are reported but not
  gated: different optimal segmentations can give different upper levels. A correct greedy shrinking cone fails.
- **Pass only.** reward = pass (0/1); no partial credit, no beat.
- **Shorter budget.** 45 minutes (2700 s), versus FrontierCS 2's 5 hours. No mid-run judge; only the final patch is
  graded.
- **Upstream counts are readable.** `/opt/pgm-tools/upstream_counts.json` holds the reference counts for the 4 dev workloads.
  Held-out workloads, seeds and counts are only in the verifier.
- **Sandbox, not a text scan.** No regex over the submission: comments or names can't fail a run. Instead the
  submission's process runs as the unprivileged `grader` user with no network; `eval.py`, held-out specs, reference
  values and `/tests` are root-only, and it can write only its own dump, which the pristine checker validates.
  The instruction's rules (no file/network/process access) are therefore enforced by isolation, not by inspection.
  `rung/pgm/tamper` tries to overwrite the reward and reference values and still scores 0 in Harbor.
- **No time gate.** Build time is recorded but not gated; only the 300 s per-workload timeout applies. An optimal
  but slower (non-streaming) method passes. `rung/pgm/paper-literal`, written from the paper only, matches the optimal
  count but builds ~7× slower than upstream; a speed gate would reject it for engineering, not fidelity.
- **Extra files are ignored.** Only tracked files are compared with the pristine tree; scratch tests and build
  dirs the agent adds don't matter (grading rebuilds from pristine + the header). `rung/pgm/scratch-files` passes.
- **Key types.** Only `uint64_t` keys are graded; the masked class is generic and signed/float paths are untested.
- **Single compiler.** Scores are deterministic for the image's g++; another compiler could change float rounding,
  so reference values must be regenerated if the image changes.
- **Framed as ordinary work, not a test.** The instruction is a plain developer request with no grading, hidden-data or
  reference wording; the repo is at `~/PGM-index` and the paper at `~/papers/`; the self-check `pgm-check` prints numbers
  next to the upstream implementation's with no targets or pass/fail. This removes the explicit cues (F1, F7, F8 in arXiv:2605.23055) but cannot hide everything: a paper plus
  one placeholder function is a recognisable setup, and both 2020 papers and repos are public. `scripts/scan_awareness.py
  <job dir>` reports how many trials say they think they are being tested (diagnostic only, not in the reward).
