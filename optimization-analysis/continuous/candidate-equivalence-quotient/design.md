# Candidate-Execution Quotient Design

## Objective

Reduce the number of RF/CO/revisit candidate executions that GenMC generates, queues,
realizes and completes. A reduction is not credited merely because a CAT evaluation is
cached or accelerated.

The first correctness scope is bounded deterministic programs under SC and local safety
properties, including assertion reachability. TSO, PSO and arbitrary CAT models remain
unchanged until separate preservation arguments exist.

## Why local same-value filtering is invalid

At a read, two currently visible writes can carry the same `(value, provenance)` while
inducing different future realizability constraints. Selecting one write and deleting the
other from `GenMCDriver::handleLoad()` can lose a later event set or read value that is
realizable only with the deleted source. Therefore the existing same-value counters are an
opportunity upper bound only.

This rules out all implementations of the form:

```text
group current RF sources by value -> enqueue one source per group
```

The representative must instead be generated for a set of allowed writes, with proof that
the exploration enumerates every relevant value/event/causal class.

## P0 algorithm: SC RVF representative exploration

The reference theorem is the CAV 2021 RVF-SMC algorithm. Two maximal SC traces are in the
same RVF class when they have:

1. the same dynamic event set;
2. the same value (and, in GenMC, pointer provenance) for every event; and
3. the same causal order between reads, where causality is the transitive closure of
   program order and reads-from.

Exploration cannot be implemented as a terminal-state hash set. It needs the four pieces
that construct a representative before all concrete RF choices are enumerated:

- `GoodW(read)`: the whole allowed same-value write set, not one selected RF source;
- an SC realizability procedure that returns a witness trace for `(events, GoodW)`;
- a per-read/per-thread causal map excluding write prefixes already covered; and
- backtrack signals when a later write can provide a genuinely new source class.

The implementation must be derived from the published algorithm and proofs. The historical
Nidhugg `reads_value_from` branch is GPLv3 and is used only to confirm architecture and
instrumentation; no source is copied into GenMC's Apache-2.0/MIT tree.

## Integration boundary

The quotient is a new exploration policy, not a replacement consistency checker.

1. Drive the interpreter to the next reads while extending deterministic write-only
   segments, recording stable dynamic event identities.
2. Maintain an annotated partial execution containing event identities, values,
   `GoodW`, causal map and the partial causal order.
3. Use a separately implemented SC realizability/closure procedure to construct a witness.
4. Replay the witness through GenMC's interpreter and execution graph.
5. Use the configured generic recursive-SC CAT/CAAT checker as the authoritative model
   acceptance path. The built-in SC checker is not used to supply verdicts.
6. If an event, operation or annotation cannot be represented, fail open to the existing
   TruSt RF-DPOR exploration for that execution subtree.

The fail-open transition must retain every existing work item. It may duplicate work but
must never delete a candidate whose class was not proved covered.

## Required invariants

- Every generated witness is a valid program execution and satisfies its `GoodW` map.
- Every bounded maximal SC execution has a represented RVF class.
- All traces represented by one class reach the same per-thread local state.
- A class is marked covered only after a witness is successfully realized and accepted by
  the configured CAT model.
- Hashes are lookup accelerators only. Equality is checked structurally before merging.
- Pointer values are equal only when both raw bits and provenance agree.
- RMW, locks, thread creation/join, optional blocks and unsupported external effects either
  have an explicit rule or trigger fail-open.
- Parallel workers share covered-class state safely or use a deterministic partition that
  proves disjoint ownership; worker-local sets alone do not prove global uniqueness.

## Verification protocol

Correctness is checked before performance conclusions:

1. independent unit/property tests for closure, witness realization and structural class
   equality;
2. exhaustive generated bounded programs comparing error reachability and reachable local
   states with existing RF-DPOR;
3. mutation-oracle and broad differential suites on the server;
4. Release plus GCC ASan/UBSan server suites;
5. a 48-worker full adapted SV-COMP run with exact verdict/category comparison.

Performance reporting separates:

- RF/CO/backward candidates offered, queued, popped and realized;
- maximal executions and quotient representatives;
- realizability calls, failures, closure iterations and witness replays;
- CAT queries and CAT time;
- CPU, wall time, peak RSS, timeout and OOM counts.

## Later model extensions

The CAV 2021 proof assumes SC and explicitly leaves relaxed-memory extensions as future
work. TSO/PSO/generic CAT cannot reuse the SC quotient merely because read values match.
Each extension needs a model-observation signature and a realizability theorem preserving
buffer visibility, coherence/from-read constraints, CAT acceptance and future enabledness.

Until then, TSO and PSO run the existing complete exploration plus the already certified
preventive pruning. Learned CAT conflicts remain a complementary P1 mechanism for removing
inconsistent subtrees, not the candidate-equivalence mechanism.
