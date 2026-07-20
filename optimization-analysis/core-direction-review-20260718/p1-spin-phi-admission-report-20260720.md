# P1 spin-loop PHI admission report (2026-07-20)

## Decision

Retain the candidate on `genmc-caat-opt-dev` and propose it for promotion after review.  A single
fail-closed change in `SpinAssumePass` converts two actual 12-GB OOMs into correct terminal results,
while reducing total CPU, suite wall time, and aggregate RSS in the full 725-task paired run.  No
terminal status, extracted verdict, complete-execution count, or blocked-execution count regresses
on the 470 tasks terminal in both lanes.

The optimization does not bound or truncate a loop.  It recognizes an existing spin-loop pattern
that the pass previously rejected solely because an LLVM PHI contained its entry seed constant.
Constants are admitted only on incoming edges from outside the loop.  A constant on any
loop-contained incoming edge remains rejected; after every backedge, the PHI must still be supplied
by the same-location, same-order polling load or an admitted PHI chain.  All existing effect-free,
CAS, and dynamic-side-effect checks remain authoritative.

## Root cause and implementation

Four LibVSync OOM logs showed a monotonically growing active execution, zero scheduler-cached
labels, only 2--6 retained work items, and 25.7--29.9 million current labels.  Dynamic inspection of
`rwlock` found repeated relaxed reads of the writer-barrier address from the same RF source.  Its
LLVM loop header contained a PHI of the form:

```llvm
%ret.0 = phi i32 [ 0, %preheader ], [ %load, %backedge ]
```

The former `isPHIRelatedToLoad()` rejected every constant without considering its incoming block.
The candidate passes the `Loop` into the recursive check, accepts a constant only when
`!loop->contains(incomingBlock)`, and retains every previous load-address/order and unsupported-value
guard.

## Correctness gates

- Positive oracle: a polling loop with a preheader constant and a load-fed backedge terminates the
  explored path as blocked (`0` complete, `1` blocked) instead of expanding forever.
- Negative oracle: a finite loop with a loop-carried constant still reaches its assertion and exits
  with the expected safety-error status 42.
- Dynamic-control oracle: a PHI seeded by an `atomicrmw`, whose next value is a body polling load,
  remains untransformed by this constant-only patch and reaches the assertion after observing the
  exit value.  The transformed IR retains the dynamic PHI and contains no inserted spin assume.
- Existing SC saver suite: all 56 configurations preserve expected complete/blocked counts.
- Existing SC liveness suite: all 5 configurations pass.
- Focused ASan+UBSan build and the positive/backedge oracles: pass with halt-on-error and leak
  detection.  The later dynamic-control oracle changes no production code and passes the same
  Release semantic gate.
- Full 725: 725 XML rows and 725 archived logs in each lane; zero launcher failures.

The full-run log audit finds zero extracted-verdict mismatch and zero complete/blocked-count
difference on all 470 common-terminal tasks.  The only two status changes are the intended
LibVSync OOM-to-`true` transitions.  Both report `No errors were detected.`:

| task | complete | blocked |
|---|---:|---:|
| `ttaslock.yml` | 36 | 58 |
| `rwlock.yml` | 23,200 | 18,918 |

## Four-task attribution gate

Configuration: recursive SC CAT/CAAT, current retained evaluator switches, one CPU and 12 GB per
task, 60 CPU seconds, four tasks per lane on disjoint NUMA-local core sets.  The clean candidate
source differs from stable only in `SpinAssumePass.cpp`.

| task | baseline | candidate | candidate CPU | candidate peak RSS |
|---|---|---|---:|---:|
| `rec_ticketlock` | OOM | OOM | 39.74 s | 12.00 GB |
| `ttaslock` | OOM | `true` | 0.097 s | 26.77 MB |
| `rwlock` | OOM | `true` | 13.86 s | 27.06 MB |
| `ticketlock` | OOM | OOM | 36.53 s | 12.00 GB |

Summed CPU falls from 144.55 to 90.23 seconds (-37.58%).  The two unsolved tasks retain the same
memory cap; their CPU changes are small and opposite in direction.  This justified OOM31 expansion.

## OOM31 gate

Configuration: 31 tasks per lane, 120 CPU seconds and 12 GB per task, 48 tasks concurrently.

| metric | baseline | candidate | change |
|---|---:|---:|---:|
| terminal / OOM | 0 / 31 | 2 / 29 | +2 terminals |
| summed CPU | 1,164.068 s | 1,093.879 s | -6.03% |
| summed wall | 1,164.825 s | 1,094.744 s | -6.02% |
| summed peak RSS | 371,999,936,512 B | 348,053,803,008 B | -6.44% |
| max task RSS | 11,999,997,952 B | 11,999,997,952 B | unchanged |

Across the 29 still-OOM tasks, CPU falls 0.92% and memory remains exactly at the same cap.  The
result is therefore not a longer-survival memory tradeoff.

## Full 725 paired result

Configuration: 24 workers per lane on disjoint NUMA nodes, one CPU, 12 GB, and 120 CPU seconds per
task.  Both lanes enable the retained primitive-cache/build/check/composition/cycle foundation.

| metric | baseline | candidate | change |
|---|---:|---:|---:|
| correct | 413 | 415 | +2 |
| `true` | 303 | 305 | +2 |
| `false(unreach-call)` | 141 | 141 | unchanged |
| TIMEOUT | 224 | 224 | unchanged |
| OOM | 31 | 29 | -2 |
| all-task CPU | 30,384.272 s | 30,283.303 s | -0.33% |
| BenchExec suite wall | 1,328.41 s | 1,322.74 s | -0.43% |
| aggregate peak RSS | 514,592,157,696 B | 490,854,608,896 B | -4.61% |
| maximum task RSS | 11,999,997,952 B | 11,999,997,952 B | unchanged |

For 470 common-terminal tasks, CPU falls 1.34%, wall falls 1.37%, and aggregate RSS changes by
+0.108%, below a meaningful process-RSS effect.  Every other status/category count is identical,
including the pre-existing 31 wrong-category rows.  There are no terminal-to-resource transitions.

## Scope and remaining opportunity

The optimization activates decisively on `ttaslock` and `rwlock`, but `rec_ticketlock` and
`ticketlock` still OOM.  Their remaining loop shapes require a separate proof and should not be
folded into this patch.  Any follow-up must again preserve the joint terminal/time/RSS gate and add
a negative oracle for the newly admitted SSA pattern before measurement.

Post-gate transformed-IR inspection identifies the next precise boundary.  Both unsolved programs
contain a polling PHI whose backedge is the same-location load, but whose outside-loop seed is a
dynamic ticket value produced by a preceding `atomicrmw`, rather than a constant.  Such an outside
incoming value also affects only zero-iteration entry and is a plausible exact generalization, but
it is deliberately not included here.  It requires its own dynamic-seed positive oracle, an
outside-value/loop-carried-value separation proof, and a fresh finite-backedge negative oracle.  A
dynamic-seed control-flow oracle is already retained: it prevents a future generalization from
blocking before a body load's exit value reaches the next header condition.

Immutable server result roots:

- `/data3/sujie/experiments/caat-optimization/p1-spin-phi-paired-4-20260720-r1`
- `/data3/sujie/experiments/caat-optimization/p1-spin-phi-paired-oom31-20260720-r1`
- `/data3/sujie/experiments/caat-optimization/p1-spin-phi-paired-725-20260720-r1`
