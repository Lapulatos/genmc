# Exact descriptor storage-reuse report (2026-07-19)

`--cat-fast-descriptor-reuse` preserves the complete descriptor and its default full-field equality,
but constructs each current descriptor in a worker-local scratch object. On a miss, the current and
cached descriptors are swapped so the old `events`, location rows, and ordered-write vector
capacities become scratch for the next query. On a hit, scratch already retains its capacity.
No hash, graph version, probabilistic equality, relation delta, or pruning is used. The tested mode
also enables exact descriptor miss-build; both require the certified primitive cache.

Correctness passes local/server Release (229 passed plus one expected Z3 skip), server
ASan+UBSan 56/56, mutation 39 rows / 5,441 full-evaluator oracle checks, and broad differential
852 matches / 12 mutually unsupported / zero mismatch over 864 pairs. The first mutation launch
hit the known nondeterministic TSO/two-worker `malloc-not-hb0.c` early-error path before any oracle
check and is invalid evidence; the complete rerun is authoritative.

The fixed-15 paired delta compares the prior cycle foundation against descriptor miss-build plus
storage reuse. All-task CPU is 416.958/414.796 s (-0.52%), completed CPU 111.982/109.791 s
(-1.96%), four-job suite wall 132.51/132.32 s (-0.14%), aggregate RSS
399,020,032/398,458,880 bytes (-0.14%), and peak RSS 27,119,616/26,992,640 bytes (-0.47%). Both
lanes retain 10 terminal / 5 TIMEOUT and exact search counts of 18,693 complete / 197,703 blocked /
zero bound.

Materialization falls 20.183/17.961 s (-11.01%) and CAT consistency 35.362/33.219 s (-6.06%).
The reported primitive scan rises 3.112/3.745 s because that timer covers stable-ID preparation
after descriptor creation, not descriptor allocation itself; total materialization includes the
removed allocation/destruction work and is the relevant phase metric. The end-to-end improvement
is real but far below the strict 10%/hard-cap gate. Do not expand to 283/725 or enable by default.
Authoritative raw evidence is under
`server-results/primitive-fast-descriptor-reuse-delta-paired-15-20260719a/`.
