# pgm: Harbor smoke test (Harbor 0.25.0, local Docker, `pgm-verifier-v1`)

| Check | Command | Result |
|---|---|---|
| Lint | `harbor lint tasks/pgm` | exit 0 |
| Oracle | `harbor run -p tasks/pgm -a oracle --force-build` | pass=1, reward=1 |
| Nop | `harbor run -p tasks/pgm -a nop --force-build` | pass=0, reward=0, gate `metric` |
| Regrade | `harbor job regrade <oracle job> -p tasks/pgm` | 0 changed, mean reward 1.000 → 1.000 |
| Agent internet blocked | oracle with a probe in `solve.sh` fetching example.com, pypi.org, github.com, 1.1.1.1 | all 4 blocked; same probe with `[agent]` network unset: 3 open, 1.1.1.1 reached (HTTP 403) |
| Grading sandbox | in the verifier image, as `grader`: write `/logs/verifier/reward.json`, read reference values, list `/tests`, write `/opt/pgm-verifier` | all `Permission denied`; reward unchanged |
| Tamper rung | `harbor run -p <task with rung/pgm/tamper as solution> -a oracle` | pass=0, reward=0, gate `metric` |
| Self-check in agent image | `pgm-check` (stub / after `solve.sh`) | all correct; stub far from upstream / equal to upstream |
| Agent image leak scan | `scripts/leak_scan_image.sh <agent image> pgm` | no `.git`, no verifier files, no `*_rules.py`; only hits are the paper text and words in the authors' own code/corpora |

Agent image (runs as user `dev`, `HOME=/home/dev`): repo at `~/PGM-index`, paper at `~/papers/`, self-check tool in `/opt/pgm-tools` (untouched upstream copy,
harness, upstream numbers for the public data). Verifier image (separate container, no network): pristine tree, harness,
`eval.py`, held-out data and reference values, all root-only; the submission's build step runs as the unprivileged
`grader` user. The repo is collected as the artifact and graded there.
