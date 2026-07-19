# Exact descriptor miss-build report (2026-07-19)

`--cat-fast-descriptor-build` reuses only the current, complete `GraphDescriptor` that
`materializeCached()` has just constructed for cache equality. On a cache miss it supplies the
current label pointers and ordered location/write positions to the existing primitive materializer,
avoiding a second graph label/location scan. Cached pointers are excluded from semantic equality
and are never dereferenced on later calls. The option requires `--cat-primitive-cache`; disabled
runs retain the old graph-rescan path.

The descriptor contains all determinants used for equality (event position, thread/index,
R/W/F/SC classes, address, RF source, RMW target, lifecycle endpoints, and ordered coherence).
No changed-query delta, inferred edge, or search pruning is introduced. A focused full-snapshot
test compares the candidate with graph rescan across RF replacement, CO mutation, append, cut with
stable-ID holes, and a new address.

Correctness passes local/server Release (228 passed plus one expected Z3 skip), server
ASan+UBSan focused 55/55, mutation 39 rows / 5,424 full-evaluator oracle checks, and broad
852 matches / 12 mutually unsupported / zero mismatch over 864 pairs. Bash reported one ignored
NUL byte while capturing a mutation command's output, but all 39 rows completed with nonzero oracle
coverage and no mismatch; it is retained as an environment diagnostic.

The valid fixed-15 delta compares the prior cycle foundation against descriptor miss-build.
All-task CPU is 416.013/416.091 s (+0.02%), completed CPU 111.039/111.094 s (+0.05%), suite wall
132.56/132.52 s (-0.03%), aggregate RSS 398,794,752/398,655,488 bytes (-0.03%), and peak RSS
27,037,696/27,058,176 bytes (+0.08%). Both lanes retain 10 terminal / 5 TIMEOUT and exact search
counts of 18,693 complete / 197,703 blocked / zero bound.

The targeted scan falls 3.024/2.756 s (-8.87%) and materialization 19.935/19.400 s (-2.68%), but
the 0.535-second internal saving is below run noise and does not improve CPU or any hard cap.
Do not expand to 283/725 and do not enable the option by default. Authoritative evidence is under
`server-results/primitive-fast-descriptor-delta-paired-15-20260719a/`.
