<div align="center">

# paper2eval

**Can agents reproduce frontier research papers?**

[Quickstart](#quickstart) · [How it works](#how-it-works) · [Add a paper](#add-your-own-paper) · [Recent work](#recent-work)

</div>

---

paper2eval turns a paper and its original repo into a [Harbor](https://github.com/harbor-framework/harbor) eval. We strip
the paper's core contribution from the codebase, and the agent has to reproduce it as faithfully, correctly and
efficiently as possible.

Each attempt is verified in code, with no LLM judge and no partial credit, against:

- **The authors' original tests**, which the agent can see.
- **Hidden inputs**, which the agent never sees.
- **An optimality target**, because a valid but wasteful method can pass the tests.

## Quickstart

You need [uv](https://docs.astral.sh/uv/) and a running Docker daemon.

```bash
uv tool install harbor
git clone https://github.com/rishiskhare/paper2eval.git && cd paper2eval

# Check the setup: the reference solution scores 1
scripts/harbor.sh run -p tasks/pgm -a oracle
```

Then point a real agent at it, with `OPENAI_API_KEY` set:

```bash
scripts/harbor.sh run -p tasks/pgm -a codex -m openai/gpt-5.6-luna --allow-agent-host api.openai.com
```

Results land in `jobs/<timestamp>/`. The trace is in `agent/`, and the grader's output is in `verifier/test-stdout.txt`.

> [!NOTE]
> On a Mac, run `brew install colima` too. Docker Desktop can't enforce the agent's network block, so
> [`scripts/harbor.sh`](scripts/harbor.sh) switches to Colima for that one command and leaves your default Docker
> context alone.

## How it works

```mermaid
flowchart LR
    A["Paper PDF +<br/>repo with one file masked"] --> B["Agent<br/>model API access only"]
    B --> C{"Sealed grader<br/>no network"}
    C -->|"all 5 checks pass"| D(["Reward 1"])
    C -->|"any check fails"| E(["Reward 0"])
```

**The agent sees** the paper as a PDF and the repo with one file reduced to a placeholder. The repo's own tests stay in
place, and its internet is blocked except for its model API.

**The grader** runs in a separate container with no network. Cheap checks come first, so bad solutions fail fast:

| # | Check | What it rules out |
|---|---|---|
| 1 | Only the allowed file changed | Editing the tests or the build |
| 2 | A clean copy of the repo compiles with that file | Code that only works in the agent's sandbox |
| 3 | Results are correct on held-out data | Wrong answers and crashes |
| 4 | The result is as good as the authors' on the paper's metric | Valid methods that aren't the paper's |
| 5 | The repo's own tests pass | Regressions |

The reference solution scores 1, and an agent that does nothing scores 0.

The instruction reads like an ordinary developer request. It names the file and the paper, and never explains the
method or mentions grading.

## Tasks

| Task | Paper | The agent must rebuild |
|---|---|---|
| [`tasks/pgm/`](tasks/pgm/) | [PGM-index](https://github.com/gvinciguerra/PGM-index), VLDB 2020 | Optimal piecewise linear segmentation |

B-trees are great, but they spend huge amounts of memory on pointers. The PGM-index replaces them with learned linear
models that map keys to their positions in sorted data. Here, the hidden inputs are new key sets, and the optimality
target is the authors' segment count.

Each task documents what it can't catch. See the [PGM-index limits](tasks/pgm/review/limits.md).

## Add your own paper

The [`paper-to-eval`](.claude/skills/paper-to-eval/SKILL.md) skill walks an agent through building a task from a paper
and its authors' repo. In Claude Code, from this repo:

```text
/paper-to-eval
```

A paper is a good fit when:

- Its contribution is one deterministic algorithm that lives in a single file or function.
- It defines "good", through an optimality claim or a tolerance a paper-only implementation can hit.
- Trusted code can check correctness, using the untouched repo or a simple brute force.
- It builds offline in minutes on Ubuntu 24.04, on amd64 and arm64.
- Its license allows redistribution.

## Recent work

- [PaperBench](https://arxiv.org/abs/2504.01848) (OpenAI) has agents replicate ML papers from scratch, graded against
  rubrics by an LLM judge.
- [FrontierCS 2](https://arxiv.org/abs/2610.29932) (Berkeley SkyLab) deletes a paper's contribution from its code and
  scores open-ended agent runs against the deleted code as the human reference.

## Repo layout

| Path | Contents |
|---|---|
| [`tasks/`](tasks/) | Harbor tasks, one folder per paper |
| [`scripts/harbor.sh`](scripts/harbor.sh) | Runs Harbor on a Docker engine that can enforce the agent's network block |
| [`.claude/skills/paper-to-eval/`](.claude/skills/paper-to-eval/) | The skill for building a new task |

## License

[`tasks/pgm/repo/`](tasks/pgm/repo/LICENSE) keeps the PGM-index authors' Apache-2.0 license. The rest of this repo
doesn't have a license yet.
