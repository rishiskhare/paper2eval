# pgm verifier changelog

- **v1** (`pgm-verifier-v1`): gates allowed files (tracked files only) → clean rebuild → trusted correctness →
  metric. Build time is recorded, not gated. Metric: bottom-level segments ≤ the authors' implementation on all 8 held-out key sets × ε ∈ {32, 64, 128}
  (the paper's optimality claim is per level; all-level totals are recorded, not gated). reward = pass (0/1).
  The agent gets the paper; held-out data are verifier-only. The submission's process runs as the unprivileged
  `grader` user and can only write its own dump; grader files are root-only.
