# Exact ordered coherence-build report (2026-07-19)

`--cat-fast-coherence-build` is an independently gated small-graph experiment. For the certified
`<=512` adaptive-offline domain it constructs dense `co` directly from each location's ordered
write row, and constructs exact `fr = rf^-1 ; co` from those rows and RF-source buckets. It does
not create the intermediate quadratic edge vectors. Larger graphs and disabled runs retain the
old materializer.

Correctness passes the complete local and server Release unit suites (226 passed plus one expected
Z3 skip), server ASan+UBSan focused 53/53, mutation oracle 39 rows / 5,441 old-evaluator checks,
and broad differential 852 matches / 12 mutually unsupported / zero mismatch over 864 pairs.
The focused adapter test compares the entire stable snapshot with the old dense-remap oracle across
RF replacement, CO reordering, event removal, and multiple locations.

The valid fixed-15 experiment compares the prior cache+primitive+checks+composition+cycle
foundation directly against the same foundation plus ordered coherence. All-task CPU is
415.186/414.842 s (-0.08%), completed-task CPU 110.216/109.834 s (-0.35%), and four-job suite wall
132.44/132.59 s (+0.11%). Aggregate RSS is 398,692,352/398,962,688 bytes (+0.07%) and peak RSS
27,025,408/27,090,944 bytes (+0.24%). Status remains 10 terminal / 5 TIMEOUT and search counts are
exactly 18,693 complete / 197,703 blocked / zero bound in both lanes.

The targeted construction does improve internal costs: primitive coherence 5.744/4.736 s
(-17.55%), relation packing 1.235/0.547 s (-55.74%), materialization 19.302/18.334 s (-5.02%), and
CAT consistency 34.490/33.521 s (-2.81%). The saved 0.97 s of materialization is too small to move
end-to-end CPU or any hard cap. Do not expand to 283 or 725 and do not enable the switch by default.
Retain it only as exact experimental evidence while the next audit targets the now-larger scan and
label/structural phases.

The first broad launch expanded an unset local shell variable and failed before producing a result;
the explicit-path `b` rerun is authoritative. Fixed-panel directory `a` is also invalid because its
candidate used a stale main Release binary and immediately rejected the new option. Authoritative
raw fixed-panel evidence is
`server-results/primitive-fast-coherence-delta-paired-15-20260719b/`.
