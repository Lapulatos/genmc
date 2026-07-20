# Research Question Card: Deagle SC-RVF direct finite backend

## Question

Can the exact value-class SC-order encoder replace native GenMC exploration on its
strictly admitted finite SC fragment, improving terminal coverage, CPU, and peak memory
simultaneously without changing reachable errors or safe verdicts?

## Hypothesis

A direct backend avoids the rejected sequential-prepass duplication.  On an admitted
program it enumerates only error-reaching RVF value classes, uses exact SC-order witness
extraction and CAT validation, and returns a verdict only after either replay-confirming
an error or proving the error-constrained formula exhausted.  Unsupported, incomplete,
budget-exhausted, or replay-failed cases fall back before claiming a verdict.

## Existing evidence

- The 283-task diagnostic classifies 107 tasks versus 5 for concrete RF+SC completion,
  with about 33% less CPU/wall and 67.7% less summed peak RSS.
- All 68 SAT assignments are concrete CAT-clean witnesses; 49 common native verdicts and
  all expected verdicts agree.
- Exact create/return/join lifecycle semantics pass four real outcome oracles.
- A sequential five-second prepass was rejected because it retained both representations:
  zero confirmed errors and about 7x summed peak RSS.

## Missing evidence

- Exhaustive error-constrained SC-RVF enumeration must distinguish proven UNSAT from
  solver unknown, assignment budget, CAT error, and replay failure.
- Safe and unsafe direct verdicts need an independent native oracle on generated finite
  programs, including branches, RF value merges, mutexes, and joins.
- A broad paired run must compare direct-backend-or-fallback against native under equal
  per-task limits and record admission, terminal transitions, CPU, wall, and RSS.
- The direct path must release finite solver state before fallback so it does not recreate
  the prepass memory regression.

## Support criteria

- Zero verdict/outcome/error mismatch in bounded exhaustive oracles and broad common
  terminals.
- Every unsafe result is replay-confirmed by the ordinary interpreter/checker.
- Every safe result comes only from exact solver UNSAT after all CAT-rejected graphs are
  blocked; no budget or timeout is treated as safe.
- At least one actual task changes timeout/resource failure to a correct terminal, or the
  admitted cohort improves CPU and peak RSS together without a terminal regression.

## Falsification criteria

- Any reachable error is classified safe, any spurious error is reported, or worker/model
  behavior changes outside the admitted fragment.
- The direct path needs native exploration to justify a safe result.
- CPU improves only by increasing memory, memory improves only by increasing CPU, or no
  actual task activates.

## Minimal next action

Add a diagnostic-only exhaustive SC-RVF search result with explicit `error`, `safe`, and
`fallback` terminals.  Validate it on the existing four-outcome join oracle and a bounded
generated finite oracle before enabling any production short-circuit.
