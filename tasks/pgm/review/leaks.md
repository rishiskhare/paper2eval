# pgm leak grep

Identifiers and comments from the cut code, grepped over the stripped tree (`tasks/pgm/repo/` at `pgm-stripped`).
The built-image listing is added in M3.

## Stripped tree

| Pattern | Hits | Verdict |
|---|---|---|
| `convex`, `cross(`, `struct Point`, `Slope` (type), `lower_start`, `upper_start`, `outside_line`, `rectangle[`, `intercept_n`, `rounding_term`, `Rourke`, `Theorem`, `feasib` | none in PLA code (`hyperrectangle` hits are the unrelated multidimensional index in pgm_index_variants.hpp) | clean |
| `hull` | `points_in_hull` counter in the stub (piecewise_linear_model.hpp:54-89) | minor hint: upstream member name kept in the stub; not fixed because pgm-stripped is tagged (would need a new [cut]-style commit) |
| `Optimal` | class name `OptimalPiecewiseLinearModel` | kept on purpose: trusted pgm_index.hpp and pgm_index_variants.hpp name this type, so the API cannot change |
| `get_intersection`, `get_slope_range` | stub declarations, plus callers in pgm_index_variants.hpp (CompressedPGMIndex::merge_slopes) | kept: API used by trusted code. The callers reveal that a segment exposes a slope range and an intersection point, not how to compute them |
| Paper link | README.md links the VLDB paper and slides | kept: README is a usage doc; the agent may also know the paper (see limits.md) |

Raw output: `for p in hull convex rectangle 'cross(' Slope 'struct Point' extreme lower_start upper_start points_in_hull outside_line Rourke optimal get_intersection get_slope_range intercept_n rounding_term feasib Theorem; do grep -rnIi -- "$p" tasks/pgm/repo; done`

The paper (VLDB 2020, PDF) is given to the agent at `~/papers/pgm-index.pdf`; it is not part of the repo.
