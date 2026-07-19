# Literature review: transferable scalability mechanisms

## Evidence boundary

The papers fall into three different problem settings: bounded symbolic verification,
SMT ordering-theory solving, and sound dynamic bug prediction. Their mechanisms are not
automatically sound for stateless model checking. The recommendations below distinguish
direct transfers from ideas that require an abstraction/refinement proof.

## 尹良泽团队：EOG 与 scheduling-constraint refinement

[On Scheduling Constraint Abstraction for Multi-Threaded Program Verification](https://doi.org/10.1109/TSE.2018.2864122)
observes that exact scheduling constraints dominate a concurrent BMC formula. It starts
from an over-approximation, validates an abstract counterexample with an Event Order
Graph (EOG), extracts small kernel reasons when the graph is infeasible, and adds their
negations as refinement constraints. A constraint-solver fallback preserves soundness
and completeness relative to the loop-unwinding bound.

The direct GenMC transfer is not “replace DPOR with CBMC.” It is to turn a failed exact
CAT/CAAT candidate into a reusable blocker over base choices. GenMC's reasoner already
reduces derived violations to `po/rf/co/...` literals; an EOG-style kernel minimizer can
remove redundant literals, canonicalize the remaining choice set, and reject any future
prefix that contains the same sufficient conflict. This affects candidate generation,
revisit insertion, and retained-history growth, rather than only evaluator time.

[Parallel Refinement for Multi-Threaded Program Verification](https://doi.org/10.1109/ICSE.2019.00074)
parallelizes refinement engines and shares refinement constraints and learned clauses.
This explains why simply increasing GenMC exploration threads is a weak scaling method:
parallel workers need distinct abstract obligations and shared knowledge, not duplicate
concrete searches. The applicable design is a concurrent kernel store plus work
partitioning by unresolved abstract choice.

[Incremental Verification of Concurrent Programs through Refinement Constraint Adaptation](https://doi.org/10.1145/3728976)
adapts prior kernel-source refinement constraints after program changes. Within one
GenMC run, the analogous opportunity is reuse across nearby revisits and graph prefixes:
key a blocker by stable event identity and base-choice source, then invalidate only the
part affected by `cut`, `removeAfter`, reads-from replacement, or coherence reorder.

## 贺飞团队：ordering theory 与 preventive propagation

[Satisfiability Modulo Ordering Consistency Theory](https://doi.org/10.1145/3453483.3454108)
avoids assigning unnecessary numeric timestamps and derives from-read orders on demand.
Its theory solver uses an event graph, incremental cycle consistency, and compact conflict
reasons. The SC/TSO/PSO extension is described in the
[TOPLAS article](https://doi.org/10.1145/3579835).

For CAT/CAAT, the transferable principle is “derive an order only when it becomes
relevant to a check.” A sparse ordering kernel can maintain program/read-from/coherence
edges and incremental reachability for the order fragment of a CAT model, while the
generic packed evaluator remains the fail-closed fallback for non-order relations,
negative difference, sets, and unsupported recursive equations. This is an evaluator
implementation technique, not delegation to GenMC's built-in SC/TSO checker.

[Interference Relation-Guided SMT Solving](https://doi.org/10.1145/3503221.3508424)
prioritizes Boolean decisions representing cross-thread interference and distinguishes
interference types. In GenMC this should first be used as a search heuristic: prioritize
reads-from/coherence choices that touch assertion, address, branch, or synchronization
slices. A heuristic may find bugs earlier without changing completeness; using it to
discard choices would require a separate proof.

[Consistency-Preserving Propagation](https://doi.org/10.1145/3563321), implemented in
Deagle, identifies fragile assignments whose addition would immediately violate ordering
consistency and propagates the opposite assignment before normal consistency checking
and conflict generation. This is the strongest explanation for Deagle's coverage
advantage that can be transferred into CAAT: before materializing a candidate graph,
ask whether one proposed `rf/co` choice completes a known forbidden order cycle. If so,
reject the choice at its decision point. The learned reason must be expressed entirely
in exact base literals and replayed by the generic reasoner.

The newer open-access FM 2026 paper
[A Refined Ordering Consistency Theory: Full Sequential Consistency and Generalized
Preventive Reasoning](https://doi.org/10.1007/978-3-032-26204-2_26) strengthens this
direction in two ways that matter to GenMC. First, it derives write-serialization and
from-read orders on demand instead of eagerly encoding them. Second, it gives four sound
but deliberately incomplete preventive patterns: Self-Reversal, Stale-Read,
RMW-Broadcast, and RF-Join. Its soundness theorem is exactly the boundary needed here:
when a partial choice matches one of these patterns, no completion can repair it; failure
to match a pattern must fall back to ordinary CAT/CAAT evaluation. This suggests a
certified SC-order fast path after the exact-kernel prototype, not a wholesale replacement
of the generic checker. The paper also explains why complete preventive propagation is
not a practical target: some RMW-chain patterns require factorially many arrangements.

The exact OOPSLA 2022 sufficient clauses sharpen the source-level mapping. For a
same-location proposal `rf(w,r)`, preventive propagation rejects it when the already
saturated order contains either `r ->* w`, or a write `w'` with
`w ->* w' ->* r`. FM 2026 lifts the endpoints to atomic-block quotient order and adds
explicit `ws` variables. In GenMC, the same test should not be coded as an SC-specific
timestamp solver. The exact recursive PSO model already exposes a direct monotone
predicate `order` and its positive recursive closure `reach`; after removing the current
choice and recomputing `fr`, a new direct edge `(u,v)` is provably fatal whenever the
counterfactual `reach` contains `(v,u)`. This formulation covers RF Self-Reversal,
FR/Stale-Read, and CO/WS Self-Reversal without changing the CAT consistency authority.

This transfer has two limits. First, SC and TSO already use independently certified
candidate filters, so the new opportunity must be measured mainly on generic PSO rather
than claimed as a universal fast path. Second, GenMC represents split RMW pairs but does
not expose the paper's general atomic-block quotient. RMW-Broadcast can be recognized
only for actual `rmw` edges, and RF-Join must remain disabled unless a real block mapping
is added. The measurement protocol is recorded in
[`p0.3-preventive-census-protocol.md`](p0.3-preventive-census-protocol.md).

## 蔡彦团队：sound prediction, dependence reduction, and pointer flow

[Sound and Efficient Concurrency Bug Prediction](https://doi.org/10.1145/3468264.3468549)
uses feasible event sets and trace-closed partial orders to predict real bugs from a
trace without blindly preserving every event/order. The later
[Reorder Pointer Flow](https://doi.org/10.1145/3597503.3623300) and
[Reduce Dependence](https://doi.org/10.1109/ICSE55347.2025.00149) works use program
semantics, points-to/SSA information, and necessary-consistent-read analysis to avoid
over-constraining inferred executions.

These are dynamic prediction methods, not exhaustive safety proofs. Their safe transfer
is therefore narrower:

- compute a property/control/address/synchronization backward slice;
- use the slice to prioritize interference decisions and compact exploration signatures;
- treat omitted dependencies as an over-approximation;
- concretely replay unsafe candidates and refine the slice when replay diverges;
- never claim safe merely because the reduced trace space is exhausted unless the
  abstraction proof establishes coverage.

Pointer-flow reordering is especially suitable for bug-first scheduling on unsafe tasks,
but not for pruning a safety proof. Necessary-consistent-read analysis is more promising
for abstraction: reads outside the necessary slice can initially share a symbolic value
class, with CEGAR splitting the class only when a candidate depends on it.

## Synthesis against the measured GenMC/Deagle gap

The adapted 725-task experiment reports:

| Tool | Correct | TIMEOUT | OOM | TIMEOUT+OOM |
|---|---:|---:|---:|---:|
| GenMC | 408 | 210 | 50 | 260 |
| GenMC+CAT | 350 | 253 | 67 | 320 |
| GenMC+CAAT | 388 | 224 | 57 | 281 |
| Deagle | 615 | 16 | 10 | 26 |

Deagle is not simply faster on every task: on common solved instances it is often slower
than GenMC. Its advantage is that symbolic order reasoning avoids enumerating a large
fraction of the executions on which GenMC exhausts time or memory. The literature and
Optimization 12 point to the same conclusion: reducing one consistency check by a few
percent cannot close a roughly tenfold resource-failure gap. GenMC needs reusable
cross-execution information and, for large control/data state, a bounded symbolic
abstraction layer.

## Evidence records

| Source | Evidence type | Transfer strength | Main limitation |
|---|---|---|---|
| Yin et al., TSE 2020 | full local paper + DOI metadata | high for bounded CEGAR/EOG | bounded loop unwinding; symbolic BMC architecture |
| Yin et al., ICSE 2019 | full local paper | high for refinement sharing | parallel engines differ from GenMC workers |
| He et al., PLDI 2021 / TOPLAS 2023 | full local papers | high for order-fragment solver | not arbitrary CAT semantics |
| Cai, Sun, He, FM 2026 | open-access full paper + artifact DOI | high for four preventive SC patterns | sound but intentionally incomplete; SC-oriented certificate needed |
| Fan et al., PPoPP 2022 | full local paper | medium for decision heuristics | SMT Boolean decisions differ from DSE choices |
| Sun et al., OOPSLA 2022 | full local paper + Deagle artifact link | high for early fragile-choice rejection | requires an exact order-theory fragment |
| Cai et al., FSE 2021 / ICSE 2024 / ICSE 2025 | full local papers | medium for slicing and prioritization | bug prediction is not exhaustive verification |
