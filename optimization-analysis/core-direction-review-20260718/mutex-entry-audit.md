# Mutex entry audit for SC-RVF

## Decision

Mutex support is the highest-coverage core expansion target, but it cannot be implemented
by whitelisting `__VERIFIER_mutex_lock` or treating its read as an ordinary atomic load.
The required first implementation is an RMW-aware SC witness model plus an adapter for
lock CAS pairs. Until that model passes an independent exhaustive oracle, mutex programs
must retain whole-program native fallback.

## Actual-workload evidence

The endpoint-normalized 725-task control census completed in server Docker with status 0.
Before refining the assume gate, 334 tasks stopped at the coarse assume/IPR reason. A
conservative experiment admitted only assumes with an empty transformed load-annotation
map. It did not create a single new quotient opportunity; instead it exposed the next
first reason:

- modeled `__VERIFIER_mutex_lock`: 326;
- external `__assert_fail`: 61;
- non-atomic load/store: 16/11;
- loop: 14;
- annotated-load assume requiring native IPR: 13;
- malloc: 5;
- RMW: 1;
- enabled: 2, both with zero quotient-disabled loads.

Because activation and class/work reductions remained zero, the assume-only production
change was rejected and reverted. The diagnostic census remains valid evidence about
the blockers hidden behind the original first-reason scanner.

## Why mutex is not a whitelist change

The interpreter lowers a lock to a `LockCasReadLabel`, a conditional
`LockCasWriteLabel`, and an assume-block when the read observes a locked value. Current
RVF handling accepts only plain atomic `Read` labels. The graph adapter rejects every
memory label whose kind is not plain `Read` or `Write`.

Mapping lock labels to plain reads/writes would be incomplete because the independent SC
solver currently lacks all three RMW constraints:

1. the successful read and write must be adjacent in the SC linearization;
2. the read must observe the immediately preceding write at that location;
3. the write exists only on the successful compare branch, while failed lock attempts
   block and may later be revisited by native IPR.

A late `rvf.reset()` at the lock is also invalid after any ancestor value class has been
merged, because it cannot recreate those ancestor RF alternatives.

## Required implementation sequence

1. Extend the independent SC problem with explicit atomic read/write pair identity and
   validate pair shape, thread order, variable identity, and success state.
2. Enforce pair adjacency and predecessor-source rules in witness generation without
   merging failed and successful lock observations.
3. Extend graph adaptation and witness replay for lock CAS labels while preserving their
   conditional write and block labels.
4. Exhaustively enumerate bounded mutex programs with uncontended, contended, failed,
   future-unlock, nested-lock, and two-lock-order shapes. Compare every reachable local
   observation, error kind, blocked execution, and worker-count class set against native
   SC.
5. Only after the oracle passes, enable mutex in the whole-program gate and run the
   actual 725 tasks directly. Count activation, RVF-reduced loads, representatives,
   complete outcomes, RF/work changes, CPU, RSS, and timeout transitions.

## Current implementation checkpoint

The independent SC problem now has an optional successful-RMW successor on a read. Input
validation requires a disjoint, immediately next, same-thread and same-variable write.
The search executes the pair as one transition: it records the read's current active
source, recomputes held variables after the read, and then installs the paired write
without allowing another event to interpose. Paired writes cannot be scheduled alone.

Nine focused solver tests pass, including malformed-pair rejection, a forced-interposition
no-witness case, and an exhaustive comparison against an independent direct SC
linearization enumerator across 21 good-write combinations. The complete server Release
unit suite passes 186/186. This checkpoint changes only the independent solver data model;
the graph adapter still rejects RMW labels and the whole-program gate is unchanged.

The next checkpoint maps successful lock CAS pairs, failed lock reads, and unlock writes;
native lock RF choices remain outside the quotient while the RVF frame survives them.
Release passes 189/189 units and 7/7 SC-RVF end-to-end tests. A contended three-thread
mutex oracle preserves both reachable read outcomes under baseline/control and RVF with
one/two workers; its no-error outcome preserves control/RVF complete counts. Adding an
uncontended lock pair to the 32-outcome IRIWish oracle preserves the same 16 reachable
outcomes and exercises 382 aggregate reduced loads.

ASan+UBSan passes 15/15 focused solver/adapter tests and both mutex end-to-end oracles;
the IRIWish mutex run preserves 16/32 and records 380 reduced loads. Sanitizer exposed and
fixed baseline interpreter UB (`&*specialDeps` on an empty pointer) in mutex lock/unlock.
It also exposed a parallel hard-error race: a task-local replay event could survive when
another worker halted first, and the pool could pop queued work after halt. Five repeated
two-worker mutex outcome runs pass after clearing the marker and checking halt before and
after queue pops. The unrelated Debug+sanitizer `fib_bench` fallback baseline still exceeds
60 seconds; Release fallback remains passing.

## Stop rule

Reject the branch before the 725 run if the independent solver cannot represent failed
lock attempts without collapsing blocked/successful behavior, or if the generated mutex
oracle finds any missing reachable observation. After correctness, reject it if the
actual workload still has zero reduced loads even if many tasks pass the static gate.

## Final actual-workload decision

The direct server/Docker run completed baseline, quotient-disabled control, and RVF for
all 725 actual tasks. Mutex support is rejected as a standalone performance branch under
the stop rule: only two tasks reach the enabled gate, and aggregate RVF attempted loads,
reduced loads, representatives, solver states, and fail-open events are all zero.

The dominant production-safe first blocker is now assume/load-annotation IPR (513 tasks),
followed by 57 non-atomic stores, 32 non-atomic loads, 32 loops, 4 malloc calls, and one
generic RMW. Control and RVF have identical status/category results on all 725 tasks.
On 398 common-correct tasks, RVF/control CPU is 1.00246 [0.99883, 1.00642], which is
zero-activation drift rather than candidate performance.

Retain the mutex/RMW solver and correctness tests as prerequisite infrastructure. Stop
isolated gate expansion. The next core decision is whether a unified annotated-read
quotient can replace native IPR inside a certified suffix while preserving ancestor
alternatives. Full analysis: `core-roadmap-after-mutex-20260718.md`.
