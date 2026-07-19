# Research question card: RVF-guided finite/Deagle solving

## Question

Can a value-first, source-lazy RF encoding reduce the first-error-model time and memory of the
finite Deagle/Yogar lane while preserving exact RF/order witnesses and native replay confirmation?

## Hypothesis

Many concrete RF selectors in the 225 admitted `pthread-wmm` skeletons share a statically identical
source-value expression. Introducing a first-layer value/provenance class can therefore reduce RF
decisions and cardinality constraints. An ordering-theory refinement can split a class when its
members have different feasible WS/FR consequences, so the final witness remains concrete.

## Current evidence

- The admitted workload contains 256,010 concrete RF selectors and 1,059,950 pairwise RF clauses;
  RF pairs are about 7.5 times the CO-pair count.
- Existing first-model queries time out before assignment 1, so reducing complete-model CAT cost is
  not sufficient.
- The outer SC-RVF prototype demonstrated local same-value reductions but failed future-write and
  integration boundaries. A solver-internal design can use incremental order conflicts to refine
  rather than globally declaring same-value sources equivalent.

## Missing evidence

- Number and distribution of exact syntactic value classes and value-plus-source-function classes.
- Whether class counts are materially smaller on the 156 baseline TIMEOUT skeletons.
- Whether value-first decisions reduce solver decisions/conflicts and first-model CPU/RSS after
  linear/native RF cardinality is already enabled.
- Whether every accepted class model can be completed to a concrete RF/CO witness and replayed.

## Support criteria

1. Solver-free census shows a nontrivial reduction in class selectors/pairs on the admitted and
   TIMEOUT cohorts, not only on a few easy tasks.
2. A no-verdict first-model prototype reduces both CPU and RSS and does not increase either metric
   on the activating cohort.
3. Concrete completion and generic CAT validation accept every replayed witness; all unsupported,
   unknown and budget outcomes fail open to native GenMC.
4. A later full gate shows fewer RF decisions/models/refinements and at least one improved terminal
   outcome without a time/memory tradeoff.

## Falsification criteria

- Fewer than 10% of concrete RF selectors or RF pairs disappear under the conservative class key
  on the baseline-TIMEOUT cohort.
- Class refinement degenerates to enumerating nearly every concrete RF source.
- First-model CPU or RSS increases materially after controlling for cardinality encoding.
- Any learned class blocker lacks an independently replayable no-completion reason or removes a
  concrete error witness.

## Minimal next action

Extend the existing solver-free representation census with conservative class counts. A class key
uses an exact constant `(width,value)` or the identical SSA value node; a provenance refinement adds
the source function (and distinguishes the initial write). This is an opportunity lower bound, not
an equivalence proof, and it must not affect encoding or verdict.
