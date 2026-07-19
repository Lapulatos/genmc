# Finite symbolic event-skeleton lane: pthread-wmm protocol

Date: 2026-07-18

## Decision

This is a new core experiment. Earlier GenMC work did not encode guards, nondeterministic
inputs, RF, and CO over one finite event skeleton. Property-directed ordering was also not
implemented. Build the symbolic lane first; ordering is a later policy over the same exact
constraints.

## Admission boundary

A task may enter the first symbolic subset only when transformed LLVM proves:

1. no CFG backedge in any reachable defined function;
2. no recursive direct-call cycle and no indirect call;
3. every modeled memory address is statically attributable in the first encoder;
4. every external effect has explicit encoder semantics;
5. the CAT model has an exact theory/fallback path.

`--finite-skeleton-stats` reports these conditions but does not change verification and
does not authorize a TRUE result. Unknown or unsupported structure falls back before any
symbolic pruning.

## Required execution path

1. Construct the per-thread finite control/event skeleton once.
2. Encode branch guards, nondeterministic inputs, event activation, RF and CO as variables.
3. Use certified CAT/CAAT propagation to reject partial assignments; unsupported CAT
   relations are checked by the complete generic evaluator.
4. Materialize every candidate error as an `ExecutionGraph` and replay it through the
   interpreter before reporting FALSE.
5. Return TRUE only after the exact finite assignment space is exhausted. A guessed loop
   or input bound can never prove safety.

## Actual-workload funnel

1. Run an admission census over every actual `pthread-wmm` task.
2. Implement the smallest subset covering a material fraction of its 179 TIMEOUT rows.
3. Pass unit/property, replay, mutation, broad CAT differential and sanitizer gates.
4. Run the complete actual `pthread-wmm` package with its normal limits.
5. Expand to other packages only if correct terminal results replace TIMEOUT/OOM without
   terminal or error-kind loss.

Raw execution counts may differ. Required equality is verdict/error and complete reachable
observation/class coverage under the admitted finite semantics.
