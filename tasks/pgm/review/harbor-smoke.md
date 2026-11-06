# pgm: Harbor smoke test (Harbor 0.25.0, local Docker, `pgm-verifier-v1`)

| Check | Command | Result |
|---|---|---|
| Lint | `harbor lint tasks/pgm` | exit 0 |
| Oracle | `harbor run -p tasks/pgm -a oracle --force-build` | pass=1, reward=1 |
| Nop | `harbor run -p tasks/pgm -a nop --force-build` | pass=0, reward=0, gate `metric` |
| Regrade | `harbor job regrade <oracle job> -p tasks/pgm` | 0 changed, mean reward 1.000 → 1.000 |
| Self-check in agent image | `pgm-check` (stub / after `solve.sh`) | all correct; stub far from upstream / equal to upstream |
| Agent image leak scan | `scripts/leak_scan_image.sh <agent image> pgm` | no `.git`, no verifier files, no `*_rules.py`; only hits are the paper text and words in the authors' own code/corpora |

Agent image: repo at `~/PGM-index`, paper at `~/papers/`, self-check tool in `/opt/pgm-tools` (untouched upstream copy,
harness, upstream numbers for the public data). Verifier image (separate container, no network): pristine tree, harness,
`eval.py`, `pgm_rules.py`, held-out data and reference values. The repo is collected as the artifact and graded there.
