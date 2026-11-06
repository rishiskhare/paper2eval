# pgm: known limits

- **The paper is given, and it describes the algorithm in prose only.** `~/papers/pgm-index.pdf` is the VLDB 2020
  paper; `poppler-utils` is in the image so it can be read offline. Section 2.1 explains the convex-hull idea and cites
  O'Rourke for the optimal streaming algorithm; turning that into code inside this repo is the task.
- **Agents may know the method.** The paper (2020) and its code are public and likely in training data. The agent's
  internet is blocked, but its memory isn't.
- **Pass only.** Reward is 0 or 1: only the header changed, every held-out segment count is at most the authors', and
  the authors' tests pass. The tests alone can't show optimality, because a valid but wasteful segmentation passes them.
- **Bottom level only.** `tests/count.cpp` counts the segments of the keys, which is the level the paper proves optimal.
  Upper levels aren't counted, because different optimal segmentations can give different upper levels.
- **No time gate.** An optimal but slower method passes, up to the timeouts in `tests/test.sh`.
- **Light isolation.** The submission runs as the unprivileged `grader` user with no network, and the reference counts
  are root-only. It runs in the same process as the checks, so code written to fool them could; this grader assumes
  agents are solving the task, not attacking it.
- **Reference counts.** `tests/reference_counts.txt` comes from running `tests/count.cpp` on the authors' header. Keep it
  in step with that file and regenerate it if the image's compiler changes.
- **Framed as ordinary work, not a test.** The instruction is a plain developer request with no grading wording, but a
  paper plus one placeholder function is a recognisable setup, and the blocked network is visible to the agent.
