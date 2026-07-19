# Deagle SC-order maximum-rank decision (2026-07-20)

## Decision

Reject the per-load maximum-rank decomposition after one strict panel.  It reduces the
syntactic latest-write expansion and slightly lowers aggregate memory, but regresses CPU
and terminal coverage substantially.  Do not repeat or run the 283-task workload.
Candidate code is removed; the paired definition and raw evidence remain on the
development branch.

## Mechanism and exactness

The baseline tests every possible class member `w` by excluding every same-location
write between `w` and load `r`, producing a near-quadratic member/write formula.  The
candidate introduces one bit-vector `latest-rank(r)` and requires:

- every active same-location write before `r` has rank at most `latest-rank(r)`;
- a selected non-initial class contains an active preceding member whose rank equals
  `latest-rank(r)`;
- selecting the initial member requires that no active same-location write precedes `r`.

Active same-location event ranks are distinct, so this is logically equivalent to the
baseline latest-write condition.  The nine-combination, atomic mutex, and 36-shape
observation oracles run both encodings and pass.  Every candidate terminal verdict agrees
with the control and every SAT witness is concrete and CAT-clean.

## Strict panel

Authoritative root:
`/data3/sujie/experiments/caat-optimization/finite-rvf-first-model-sc-order-max-rank-panel-20260720-r1`.
It contains two complete 15-row XML lanes, 30 logs, a zero-status manifest, and a
completion marker.  Tasks use one CPU, 4 GiB, and 120 seconds.

| Metric | Baseline | Maximum rank | Candidate/control |
|---|---:|---:|---:|
| summed CPU | 1,177.719 s | 1,250.784 s | 1.06204 |
| summed wall | 1,177.968 s | 1,251.093 s | 1.06208 |
| summed per-task peak RSS | 2,854,498,304 B | 2,720,595,968 B | 0.95309 |
| SAT / UNSAT / unknown | 4 / 3 / 8 | 2 / 3 / 10 | -- |
| classified | 7 | 5 | -- |

Only five tasks terminate in both lanes.  Their CPU/wall geometric-mean ratios are
1.14800/1.14911 and summed ratios are 2.08282/2.08224.  Two tasks regress, while common
RSS is effectively neutral (geometric mean 1.00052, sum 1.02231).  The candidate loses
the baseline SAT classifications for `mix035.oepc.yml` and `rfi005.yml`.

## Interpretation

Reducing formula size does not reduce solver search here.  The auxiliary maximum rank
adds a weakly constrained bit-vector and converts direct member/ordering conflicts into
indirect bounds, making SAT search and conflict learning worse.  The modest memory
reduction is an unacceptable memory-for-time/coverage tradeoff.  Further local
latest-write rewrites are stopped; the next useful step is an end-to-end validated SAT
prepass with native fallback using the already proven baseline encoding.
