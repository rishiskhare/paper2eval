---
name: paper-to-eval
description: Turn a paper and its authors' repo into a Harbor eval task. The agent gets the paper and the repo with the main method cut out, and is graded pass/fail by code on whether it re-implemented the method faithfully. Use when asked to build a paper-reproduction or masked-method eval task for a new paper.
---

# Turning a paper into an eval task

You start with a paper and the authors' repo at a fixed commit. You end with `tasks/<t>/`: a Harbor task that has
been checked end to end. `tasks/pgm/` (PGM-index, VLDB 2020) is the worked example. Read its `tests/test.sh`,
`tests/count.cpp` and `review/limits.md` first.

## The idea

The agent gets the paper and the authors' code with the main method removed, and puts the method back. The paper is
the spec, so we don't write our own description of what to build. Grading checks that the result builds, is correct,
and is as good as the authors' version on the measure the paper cares about.

These choices are settled, so don't revisit them for each task:

- **What the agent sees.** The PDF at `~/papers/<t>.pdf`, with `poppler-utils` installed, and the repo at `~/<Repo>`
  with one file reduced to a placeholder. The repo's correctness tests stay in place. There's no check tool, no target
  numbers and no extra workloads.
- **The instruction.** A short request in a developer's voice: which file, which paper, and only the constraints the
  grader enforces. It never explains the method or mentions grading. Anti-cheat rules give the setup away.
- **Scoring.** Code only, no LLM judge. Reward is 1 or 0, with no partial credit, no bonus for beating the authors, no
  speed target and no scanning code for banned words.
- **Order of checks.** Only the allowed file changed → a fresh tree plus that file compiles → held-out correctness →
  held-out metric → the repo's own tests. Run cheap checks first, because bad solutions should fail in seconds and a
  weak method can make the test suite slow or memory-hungry.
- **Isolation.** The verifier is a separate container with no network. Submitted code runs as an unprivileged `grader`
  user via `setpriv`, and verifier files are root-only.

Keep the task small. A task ships only what Harbor needs. Process tooling, notes and test variants don't belong in it.

## Step 0: check the paper fits

Stop and tell the user if any of these fails.

| Question | Good answer |
|---|---|
| Is the contribution one deterministic algorithm? | It lives in one file or function the repo calls. |
| Does the paper define "good"? | It claims optimality, or a paper-only implementation can land within a stated tolerance. |
| Can trusted code check correctness? | The untouched repo or a simple brute force can verify the output. |
| Does it build offline? | On Ubuntu 24.04, in minutes, on amd64 and arm64. |
| Does the license allow redistribution? | Record URL, commit and license in `review/`, never inside `repo/`. |
| Is it already in a benchmark? | Search FrontierCS, ResearchCodeBench, LMR-Bench, SciReplicate-Bench, PaperBench and AlgoTune. |
| How likely is memorisation? | Note the paper's year. Newer is better. |

If the code relies on details the paper never states, the target must be a tolerance or an optimality bound, never
"match the authors' output exactly".

## Step 1: prepare the repo

1. Copy the repo into `tasks/<t>/repo/` without `.git`, and add nothing. A provenance file tells the agent it's a
   vendored copy.
2. Delete only what hands over the answer: other implementations of the method, generated outputs, and anything that
   prints its exact results. Grep for the method's key identifiers. Keep tests that only check correctness. Remove
   only cases that assert the method's exact outputs.
3. Make the smallest fixes needed so every target builds with the agent image's compiler. Use the normal build
   (`cmake -S . -B build && cmake --build build`), not just the files the grader needs.
4. Replace the method with a placeholder that compiles and returns a valid but poor result, so it fails the metric and
   not the build. Keep every signature the repo and tests use, and leave no hints about the method.
5. Save the authors' file as a patch in `solution/reference.patch`.

## Step 2: write the grader

Copy `tests/test.sh`, `tests/Dockerfile` and the shape of `tests/count.cpp` from `tasks/pgm`.

- **Allowed file.** Fail if any original file is edited or deleted, or if the allowed file is a symlink. Ignore new
  files, because grading rebuilds from the original tree plus that one file.
- **Held-out check.** A small program, compiled against the submission, that generates a few datasets from fixed seeds
  and probes the edges of what the paper guarantees. It verifies results with simple trusted logic and prints the metric.
- **Metric.** Compare with the authors' numbers, saved by running the same program on the reference. Measure the method
  directly: pgm counts `make_segmentation` output, because the index can add a segment of its own. Use no slack if the
  paper claims optimality, and the smallest justified slack otherwise.
- **Correctness must cover every caller.** The repo's tests do this for free: in pgm they caught a broken
  `CompressedPGMIndex` that the main path never touched. Compile them directly with `-O2`, which took about a minute
  where the default CMake build took about six.
- **Rewards.** Write 0 first, write 0 again on every failure, and write 1 only at the end, so submitted code can't leave
  a stale reward behind. Put a `timeout` on everything the submission runs.
- **Sandbox.** Never run the submission as root. If you change permissions, rerun the oracle.

## Step 3: package for Harbor

- **Agent image:** pinned base digest, `USER dev`, `HOME=/home/dev`, repo and PDF under `/home/dev`, `poppler-utils`.
- **Verifier image:** pinned digest, the original tree, the check program and saved metrics, a `grader` user, and
  `chmod -R go-rwx` on all of it.
- **`task.toml`:** `[agent] network_mode = "allowlist"` with `allowed_hosts = []`, and `[verifier] network_mode =
  "no-network"` with `environment_mode = "separate"`.
- **Running:** use `scripts/harbor.sh`. Docker Desktop's kernel lacks `CONFIG_NFT_FIB_INET`, so Harbor refuses the
  allowlist there, and the script uses Colima on a Mac for that command only.
- **Model access:** Harbor can't always work out the agent's endpoint and warns when it can't. Add it with
  `--allow-agent-host`, for example `api.openai.com`, and allow nothing wider.

## Step 4: prove the grader

Run the real `test.sh` in the real verifier image on these cases. Keep them outside the repo.

| Case | Expected | What it shows |
|---|---|---|
| Authors' code, run twice | 1, identical | The reference passes, and grading is repeatable. |
| Authors' code plus scratch files | 1 | New files and ordinary words in comments don't hurt. |
| Written from the paper | 1 | The target is fair. |
| Placeholder | 0 at metric | The starting point fails. |
| Correct but weaker method | 0 at metric | The metric catches it. |
| Off-by-one or too-loose bound | 0 at correctness | A low metric can't hide a wrong answer. |
| Wrong only on a secondary caller | 0 at repo tests | The whole API is covered. |
| Edited or deleted original file, symlink | 0 at allowed file | Changes outside the file are caught. |
| Overwrites the reward or reference | 0 | Tampering can't change the score. |

**The written-from-the-paper case checks the grader, not the agent.** Write it from the paper's text, not the authors'
file, and preferably as a different algorithm. If it fails on something the paper doesn't specify, loosen the target to
what the paper pins down, then confirm the weaker method still fails. Don't tune the target to the authors' unstated
choices. If it fails on a real bug, fix that bug and rerun.

## Step 5: validate

The task is done when all of these hold:

- Every case above behaves as expected.
- `harbor lint tasks/<t>` exits 0.
- Through `scripts/harbor.sh`, the oracle scores 1 and `nop` scores 0, both with no exceptions.
- As `dev` in the agent image: `~/<Repo>` and `~/papers` exist, the normal build and tests pass, and
  `pdftotext ~/papers/<t>.pdf -` prints the paper.
- The image has no `.git`, verifier files, held-out data or grading words, apart from hits in the authors' own code.
- A temporary probe in `solve.sh` can't reach example.com, pypi.org, github.com or 1.1.1.1.

Record the limits in `review/limits.md`: memorisation, names that hint at the method, coverage, isolation, and what the
agent can infer. Then have someone with no context try to break the task.

## Step 6: run trials

Run trials only with credentials the user provides, ideally a project key with a spending limit.

- Run each model several times on one setup and report pass rates. A single run proves little.
- Judge results by the grader. In pgm, an agent twice reported the optimal method when it had neither.
- Read the traces for signs the agent suspects a test. Report that next to the reward, never as part of it.
- Before committing a job, search it for secrets and drop core dumps and large leftovers. Agent logs and artifacts are
  captured as they are.

## Lessons from pgm

- Deleting `test/` broke the CMake build and left the agent with no feedback. Correctness tests stay: the placeholder
  passes them too, so they catch bugs without revealing the method.
- A custom harness grew to about 500 lines and still missed the `CompressedPGMIndex` bug. The authors' tests caught it.
- A banned-words scan could zero correct code over a comment. Isolation replaced it.
- A check tool that showed the authors' numbers gave the agent more than paper plus scaffold, and it broke under
  `USER dev`. It was removed.
- `~` paths failed while the image ran as root. Use `USER dev` and set `HOME`.
- `morton_nd.hpp` needed `#include <cstdint>` on the image's compiler. Build every target.
- `UPSTREAM.md` in the agent's tree contradicted the placeholder and revealed the setup. Keep provenance in `review/`.
- `pdftotext` output matched the shipped `.txt` byte for byte, so ship only the PDF.
- A failing check wrote no new reward, so a tampering submission kept its forged 1. Every failure now writes 0.
- With the block in place, agents tried to `curl` the authors' code. Keep the block on for every scored run.
