# P0 regional SC-RVF native-ancestor frontier decision

## Defect isolated from the rejected loop admission

Manually unrolling the first loop counterexample showed that the defect was not caused by LLVM
loop identity.  For shape `000100` and observable state `2121`, native RF-DPOR reaches the error
after 12 complete executions, while the prior whole-program SC-RVF path incorrectly reported safe
after 23.  Native reproduced the error for explicit arbitrary-scheduling seeds 1, 2, 3, 7, and 42;
SC-RVF missed it for all five, ruling out random exploration order as the explanation.

The first quotient at read `(2,2)` grouped same-value sources while two earlier native reads in the
other thread could still acquire backward revisits from future writes.  The representative children
did not own the Cartesian product between the quotient choice and those native ancestor revisits.
Disabling only this first merge restored the error; disabling the later merge at `(2,6)` did not.

## Conservative completeness repair

Each RVF frame now carries the exact positions of native reads that were synthesized as singleton
GoodW constraints when a quotient region opened.  If a later write to one of their addresses
materializes, the speculative region is revoked and its untouched native entry is replayed.  This
uses the dynamic execution frontier rather than a static program gate.  A first attempt to trigger
only from the representative graph's revisitable list was ineffective: that list was empty at every
future write precisely because the missing native branch was absent from the representative state.

The current-read verification constraints are also separated from inherited RVF-owned GoodW state.
Synthesized native singleton constraints are temporary inputs to the current `verifySC` call and are
not incorrectly published as permanent child-frame ownership.

## Transaction-result accounting repair

The first 20-shape unrolled run recovered every observable error but exposed 81 safe-result
inconsistencies for shape `001001`: RVF n1/n2 reported 76/78 complete executions, versus 118 in the
fully native traversal.  The difference was scheduling-dependent accounting, not an equivalence
partition difference.  Results accumulated before the transaction opened were buffered together
with speculative results and discarded on revocation.

The pool now splits every region-opening task result at the exact transaction boundary:

- the pre-region result is a durable prefix;
- the post-open result is speculative;
- commit publishes durable plus speculative results;
- revocation publishes only the durable prefix and then schedules native replay.

The region-opening task is registered as an outstanding descendant before its token is returned, so
a revocation request cannot bypass durable-prefix publication through an immediate replay.

## Authoritative server evidence

All experiments used the GCC 13/LLVM 15 server Docker build with 8 CPUs and a 12 GiB hard memory
limit.  This is intentionally above the earlier 4 GiB diagnostic cap.

- Focused shape `001001`: 81 states, 324 invocations, zero violations.  All 80 safe states report
  exactly 118 complete executions in native/RVF and n1/n2; the one reachable error agrees in all
  four lanes.
- Full unrolled oracle: 20 canonical shapes, 1,620 states, 6,480 invocations, zero status,
  error-category, safe-warning, late-fail-open, or n1/n2 violations.
- The 156 RVF-reduced cells from the pre-fix address-frontier run remain 156 after result splitting;
  the accounting repair did not remove valid quotient activation.
- Aggregate process time is 110.120 s native versus 121.645 s RVF for n1 (+10.5%), and 105.827 s
  versus 113.748 s for n2 (+7.5%).  These tiny generated programs are startup-heavy, but the result
  is still negative performance evidence: this mechanism is not promoted as an optimization yet.
- Five focused `RegionalRvfTransaction` unit tests pass.  An attempted target named `unit` failed
  because the generated build exposes `unit_tests`; the corrected target built successfully.
- Re-enabling regional loop admission after the repair passes the formerly decisive loop oracle:
  6,480/6,480 calls, zero violations, and 156 reduced cells. This supersedes the unrepaired loop
  rejection but does not by itself establish workload benefit.

Raw artifacts:

- `/data3/sujie/experiments/caat-optimization/p0-rvf-native-ancestor-20260719/unroll2-shape8-prefix-split-r1/`
- `/data3/sujie/experiments/caat-optimization/p0-rvf-native-ancestor-20260719/unroll2-20shape-prefix-split-r1/`
- rejected predecessor:
  `/data3/sujie/experiments/caat-optimization/p0-rvf-native-ancestor-20260719/unroll2-20shape-address-frontier-r1/`
- repaired loop gate:
  `/data3/sujie/experiments/caat-optimization/p0-regional-loop-20260719/loop2-20shape-native-frontier-r1/`

## Decision

Retain the dynamic native-ancestor frontier and transaction result split on the development branch
as a correctness foundation.  Do not merge them into `genmc-caat` or claim an end-to-end speedup:
the exhaustive unrolled and loop oracles are now clean, but only 156/1,620 cells retain a quotient
in each and aggregate unrolled time is slower. The next P0 gate must find a nonzero region in an
actual workload and reduce offered/queued RF work or realized prefixes without increasing either
time or memory.
