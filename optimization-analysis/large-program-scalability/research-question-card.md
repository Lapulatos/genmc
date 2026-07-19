# Research Question Card: scaling GenMC+CAT/CAAT beyond small programs

## Question

How can GenMC preserve exact concurrent-execution verification while reducing the
TIMEOUT/OOM gap on large bounded C programs, especially by learning and reusing compact
order-conflict kernels instead of enumerating and retaining each concrete execution
graph independently?

## Current evidence

- On the adapted 725-task, 60-second SC comparison, GenMC solves 408 tasks and has 210
  timeouts plus 50 OOMs; Deagle solves 615 with 16 timeouts plus 10 OOMs.
- On common solved tasks Deagle is slower than GenMC, so GenMC's small-instance engine is
  not intrinsically slow. Its dominant failure is loss of coverage as execution/event
  choices grow.
- Optimization 11 reduces CAT/CAAT work per candidate and is retained. Optimization 12
  reduces offline closure time by 9.5% and snapshot-equivalent state by 2.2% in its
  mechanism profile, but leaves 1,261,169 candidate queries unchanged and does not meet
  the frozen aggregate CPU retention gate.
- Existing `--nthreads` experiments show little wall benefit because workers parallelize
  concrete exploration without learning enough reusable information across workers.

## Hypothesis

A two-level exploration can improve large-program coverage:

1. GenMC continues to execute/replay concrete candidates and remains the final witness
   authority.
2. A compact event-order/refinement layer learns sufficient conflict kernels over
   program order, reads-from, coherence, lifecycle, and property-relevant control/data
   dependencies. A learned kernel blocks an entire family of future prefixes, and is
   shared across workers.

The expected gain comes primarily from reducing explored/retained equivalence classes,
not from another constant-factor optimization inside one CAT evaluation.

## Evidence required

- Per-task counters for complete executions, partial prefixes, revisits, rejected
  candidates, learned kernels, kernel hits, active events, graph bytes, and retained
  history bytes on the 260 GenMC TIMEOUT/OOM tasks.
- A replayable proof that every learned blocker is implied by the exact CAT model and
  the concrete program semantics.
- Ablations separating EOG validation, kernel generalization, kernel reuse, dependency
  slicing, and cross-worker sharing.
- Coverage and resource curves at 10/30/60/180 seconds, not solved-only timing alone.

## Support criteria

- Zero wrong verdicts and zero safe-execution-count mismatch on the frozen 288-program
  SC/TSO/PSO differential/oracle suite.
- Every unsafe result has a concrete GenMC replay; every safe result is produced only
  after the exact exploration/refinement fixpoint required by the selected bounded
  semantics.
- At least 20% fewer explored candidates or retained prefix states on the targeted
  TIMEOUT/OOM cohort, with at least 10% fewer TIMEOUT+OOM rows at 60 seconds.
- Peak RSS does not increase by more than 5% on common completed tasks.

## Falsification criteria

- Learned clauses mostly reject candidates only after full graph construction.
- Kernel storage grows proportionally to concrete executions and merely moves the OOM.
- Dependency abstraction needs frequent refinement and increases total explored states.
- Cross-worker sharing adds contention without reducing total candidate queries.
- The method is sound only for bug finding and cannot support bounded safe results.

## Minimal next action

Instrument exact CAT/CAAT rejection explanations as canonical base-choice kernels and
run a read-only census on the 96-task sample plus a stratified subset of the 260
TIMEOUT/OOM tasks. Do not prune yet; measure kernel size, duplication, subsumption, and
the earliest prefix at which each kernel would have fired.
