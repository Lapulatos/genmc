# P0.7d1 streaming lazy-cycle traversal protocol

Date frozen: 2026-07-16
Baseline: `959f8697cfd3b20d47a4ca9d925e5e7e8ffdc508`

## Question

Can exact cycle checking consume the analyzer-certified relation expression as a
deterministic edge stream, avoiding per-source successor vectors, deduplication, sorting,
and post-cycle composition enumeration while retaining P0.7b's persistent-state savings?

## Semantic theorem

For cycle existence, a finite relation is equivalent to the directed multigraph obtained
by emitting its extensional derivations in any deterministic order: parallel copies of an
edge neither create nor remove a directed cycle. Standard three-colour DFS may therefore
process each emitted target immediately. A grey target is a valid back edge; a white
target is visited recursively; a black target and later duplicate copies are ignored.

The emitter must be deterministic for the same normalized model and base values. Every
reported witness must be a closed path whose consecutive pairs are members of the exact
root relation. It need not equal the generic evaluator's lexicographically first witness.
Explanation and oracle modes continue to disable lazy evaluation and retain their current
witness contract.

## Minimal implementation boundary

- Change only the lazy cycle evaluator and focused tests.
- Make edge emission interruptible so a discovered cycle stops remaining base, union,
  intersection, and composition enumeration.
- Remove per-source successor collection, generation marks, deduplication, and sorting.
- Keep the analyzer grammar, composition-cost selector, base values, incremental state,
  checkpoint/rollback logic, and built-in checker isolation unchanged.

## Expected time and space effects

- Remove one dynamically growing successor vector and sort for every visited source.
- Remove generation-mark writes and unique-edge insertion branches. Duplicate derivations
  perform a colour check instead; P0.7b2's candidate/unique ratio is 1.304, so this
  tradeoff is measured.
- On inconsistent graphs, stop the emitter at the first discovered DFS cycle instead of
  finishing all successors of the current source. Emitted candidate count can only stay
  equal or decrease.
- On consistent graphs, primitive/composition derivations are unchanged; this prototype
  does not solve repeated macro-edge generation and must not claim that it does.
- Auxiliary cycle state becomes `O(N + D)` for colour, parent, witness, and bounded
  expression recursion depth `D`, removing the retained P0.7b `O(E_path)` successor rows.
- Persistent values, undo entries, and checkpoint-equivalent bytes are unchanged exactly.

## Correctness gates

1. Random finite relations compare cycle existence against the materialized root for all
   admitted operators, including duplicate derivations and self-loops.
2. Every non-empty lazy witness is checked edge-by-edge against the materialized root and
   repeated evaluation returns the identical witness.
3. Incremental insertion and rollback retain exact verdicts.
4. Complete Release unit/property, 39-row mutation oracle, and 288x3 broad differential
   pass with zero mismatch.

## Performance and retain gate

Run the same 96-task, SC/TSO/PSO, four-repetition, swapped-core, 48-worker matrix with one
CPU, 4 GiB, and 60 seconds per cell.

Retain only if all are true:

- no opposite terminal verdict, common-safe execution mismatch, lost correct terminal,
  or new OOM;
- aggregate common-terminal CPU 95% CI upper <= 1.01;
- TSO and PSO CPU point ratios <= 1.02;
- large-event process RSS <= 1.02 and peak snapshot-equivalent bytes equal 1.00;
- and at least one measured benefit: aggregate CPU point <= 0.995, large-event RSS <=
  0.95, or at least 15% fewer candidate edges together with large offline time <= 0.97.

Otherwise restore the lazy evaluator/tests to baseline and retain only the experiment
record.

