# pgm: verifier vs upstream evaluation

| Aspect | Upstream (`test/tests.cpp`, paper §7) | This verifier |
|---|---|---|
| Correctness | "Segmentation algorithm" test: each segment predicts every covered key within ε; `PGMIndex` tests: `search(k)` range contains `lower_bound(k)` on random data | Trusted `PGMIndex<uint64_t, ε, 4>::search` (pristine `pgm_index.hpp`, separate process) on every key, key±1, gap midpoints, 0, 2^64-2 and 4096 random probes; range width ≤ 2ε+2, `lower_bound` rank inside `[lo, hi]` |
| ε values | Many (catch generators, 16..256 and templated) | 32, 64, 128 |
| Key types | int, uint32/64, float, double | uint64 only |
| Data | Uniform / normal / random, sizes up to 10^6 | 8 held-out generators (uniform in [2^62, 2^63), lognormal, clustered, heavy-tailed gaps, keys within 2^40 of 2^64, dense runs, 50 groups separated by ~2^50 gaps, mixed), 10^6 keys each, plus 4 public dev workloads |
| Metric | Paper: index space (segments × 16 B) and query time vs. FITing-tree, B+-tree | Bottom-level segments (the level the paper proves optimal); total over all levels is reported, not gated. Build and query time not scored |
| Comparison | Paper reports absolute numbers | Pass if ≤ the upstream implementation's count on every held-out workload and ε (pass-only) |

Differences that matter:
- Query time is not scored: it depends on the machine; the bottom-level count is deterministic and is what the optimal algorithm minimises.
- Recursive levels are built by the trusted `PGMIndex::build` with ε_rec = 4, so a better segmentation also shrinks upper levels.
- Duplicate keys are not exercised (workloads are unique), matching `PGMIndex`'s documented use.
- `make_segmentation_par` / OpenMP is not used; harness compiles without `-fopenmp`.
