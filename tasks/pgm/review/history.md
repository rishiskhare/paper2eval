# pgm history

Upstream: https://github.com/gvinciguerra/PGM-index @ c6fcf3d (Apache-2.0). Allowed file: `include/pgm/piecewise_linear_model.hpp`.

## Tags

| Tag | Commit | Meaning |
|---|---|---|
| `pgm-upstream` | `[upstream] pgm: import upstream c6fcf3d unmodified` | Upstream tree byte-identical, plus repo/UPSTREAM.md |
| (no tag) | `[leak] pgm: remove files that leak the method` | Deletes test/ (the segmentation test is a ready oracle for the cut code) |
| `pgm-stripped` | `[cut] pgm: strip optimal piecewise linear segmentation` | Convex-hull PLA removed; flat-segment stub added. What the agent sees |
| `pgm-reference` | `[ref] pgm: restore reference` (branch `ref/pgm` only) | Upstream piecewise_linear_model.hpp restored byte-for-byte |
| `pgm-verifier-v1` | `[review] pgm: ...` (after sanity and Harbor checks) | Frozen verifier used for agent trials |

No `[build]` commit: the library is header-only and compiles as-is.

## Diff commands

```sh
# What we changed from the authors' repo before the cut ([build]/[leak] only)
git diff pgm-upstream pgm-stripped^ --stat -- tasks/pgm/repo/
# What exactly was cut (= tasks/pgm/evaluator/strip.patch)
git diff pgm-upstream pgm-stripped -- tasks/pgm/repo/
git diff pgm-stripped^ pgm-stripped -- tasks/pgm/repo/        # the cut alone, without the [leak] deletions
# What the reference restores (= tasks/pgm/evaluator/reference.patch)
git diff pgm-stripped pgm-reference -- tasks/pgm/repo/
# The reference is exactly the authors' code (must be empty)
git diff pgm-upstream pgm-reference -- tasks/pgm/repo/include/pgm/piecewise_linear_model.hpp
# How an agent's patch differs from the paper
git diff pgm-reference trial/pgm/<agent>-<n> -- tasks/pgm/repo/include/pgm/piecewise_linear_model.hpp
# A sanity rung
git diff pgm-stripped rung/pgm/<name> -- tasks/pgm/repo/
# Everything above, checked automatically
scripts/check_history.sh pgm
```
