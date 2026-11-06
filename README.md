# paper2eval

paper2eval turns research papers into [Harbor](https://github.com/harbor-framework/harbor) eval tasks. An agent gets a
paper and the authors' code with the main method cut out, and has to put the method back. The paper is the spec, and
code decides the score.

## How a task works

**The agent sees** the paper as a PDF and the repo with one file reduced to a placeholder. The repo's own tests stay in
place, and its internet is blocked except for its model API.

**The grader** runs in a separate container with no network. It scores 1 only if all of these pass, and 0 otherwise:

1. Only the allowed file changed.
2. A clean copy of the repo compiles with that file.
3. Results are correct on held-out data.
4. The result is as good as the authors' version on the measure the paper cares about.
5. The repo's own tests pass.

The tests check correctness, so a valid but weak method can pass them. The held-out metric is what separates it from
the paper's method.

## What's here

| Path | Contents |
|---|---|
| `tasks/pgm/` | [PGM-index](https://github.com/gvinciguerra/PGM-index): re-implement the optimal piecewise linear segmentation from the VLDB 2020 paper. See `review/limits.md` for what a pass doesn't prove. |
| `scripts/harbor.sh` | Runs Harbor on a Docker engine that can enforce the agent's network block. |
| `.claude/skills/paper-to-eval/` | A skill that guides an agent through building a new task from a paper. |
| `jobs/` | Past runs, including the sample trace below. |

## Setup

Requires [uv](https://docs.astral.sh/uv/) and a running Docker daemon.

```bash
uv tool install harbor
```

On a Mac, also `brew install colima` and use `scripts/harbor.sh` in place of `harbor`.

## Running a task

Check the task with the reference solution, which should score 1:

```bash
scripts/harbor.sh run -p tasks/pgm -a oracle
```

Run a real agent with your model credentials:

```bash
scripts/harbor.sh run -p tasks/pgm -a codex -m openai/gpt-5.6-luna --allow-agent-host api.openai.com
```

Results land in `jobs/<timestamp>/`. The trace is in `agent/`, and the grader's output is in `verifier/test-stdout.txt`.

## Sample trace

[`jobs/2026-11-06__16-11-25/`](jobs/2026-11-06__16-11-25/) is a full run of GPT-5.6 Luna through Codex on `tasks/pgm`. It
built the repo and ran its tests, but it scored 0: on the held-out `gaps` data at ε=32 it used 5,001 segments, where the
authors' code uses 3,545.

- Agent trace: [`trajectory.json`](jobs/2026-11-06__16-11-25/pgm__Cmk2due/agent/trajectory.json) and
  [`codex.txt`](jobs/2026-11-06__16-11-25/pgm__Cmk2due/agent/codex.txt)
- Grader output: [`test-stdout.txt`](jobs/2026-11-06__16-11-25/pgm__Cmk2due/verifier/test-stdout.txt)
