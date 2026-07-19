# Finite symbolic lane: actual pthread-wmm admission census

Date: 2026-07-18

## Conclusion

Proceed with the first static-address symbolic subset. It covers 225/283 actual
`pthread-wmm` tasks and 156/179 of the baseline TIMEOUT cohort (87.15%). Expanding dynamic
address support is not the first implementation dependency.

The package has no baseline OOM rows. Its resource-success gate is therefore specifically
TIMEOUT-to-correct-terminal conversion.

## Frozen input and execution

- actual tasks: 283 YAML tasks under `pthread-wmm`;
- container: `genmc15noble:sujie`, LLVM 15.0.7, GCC 13 Release binary;
- binary SHA-256 for authoritative rerun B:
  `7bfc7622a8b40691643e1a0e44fc6a8c60dcc62166d588a658eb108dc7744f45`;
- run mode: compile and transform only, then `--finite-skeleton-stats-only`;
- limit: 15 CPU seconds, 4 GiB, one core per task, 48 parallel tasks;
- result directory:
  `server-results/finite-skeleton-pthread-wmm-20260718b/`.

BenchExec classifies the stats-only rows as `ERROR` because the process deliberately exits
before printing a verification verdict. This is expected and is not correctness evidence.
All 283 logs contain exactly one complete `Finite skeleton:` record, and the manifest
status is zero.

## Admission result

| Baseline class | Encodable | Dynamic-address fallback | Total |
|---|---:|---:|---:|
| TIMEOUT | 156 | 23 | 179 |
| false(unreach-call) | 55 | 35 | 90 |
| true | 14 | 0 | 14 |
| Total | 225 | 58 | 283 |

All 283 tasks have:

- zero CFG backedges;
- zero indirect calls;
- zero reachable recursive functions;
- zero unsupported external calls;
- zero LLVM atomic-RMW instructions after transformation.

The only first-subset blocker is one dynamic memory address in each of 58 tasks. The
blocked tasks are concentrated in `safe` (50), `rfi` (5), and `thin` (3); none of the
`mix` tasks is blocked.

## Static size

Counts below include only functions reachable from `main`, direct calls, and transformed
thread-create entry operands.

| Metric | Minimum | Median | Maximum | Sum |
|---|---:|---:|---:|---:|
| reachable functions | 3 | 4 | 5 | 1,175 |
| LLVM blocks | 7 | 185 | 519 | 52,870 |
| LLVM instructions | 63 | 750 | 1,965 | 208,462 |
| conditional branches | 2 | 127 | 362 | 36,180 |
| loads | 7 | 196 | 543 | 55,534 |
| stores | 8 | 53 | 126 | 14,242 |
| nondet calls | 0 | 2 | 8 | 811 |
| assume calls | 1 | 1 | 1 | 283 |
| thread creates | 2 | 3 | 4 | 892 |

These are LLVM-site upper bounds, not final active-event counts. Guard variables must
control event activation so mutually exclusive sites are not materialized together.

## Decision boundary

Implement guard/nondet/event-activation and RF/CO encoding for the 225-task exact static-
address subset. Every unsupported case falls back before symbolic search. Do not report
TRUE until the finite assignment space is exhausted and every CAT relation is either
encoded exactly or checked by the complete generic evaluator. Replay every error model
through the interpreter before reporting FALSE.

## Pointer-free IR construction rerun C

After adding the immutable function/block/SSA/event IR, the complete 283-task census was
rerun in the server Docker/BenchExec environment. The authoritative result is
`server-results/finite-skeleton-pthread-wmm-20260718c/`; its manifest status is zero and
all 283 logs contain both the structural record and the IR-construction record.

- 225/225 static-address tasks report `built=true`;
- 58/58 dynamic-address tasks report `built=false` with only
  `dynamic-memory-address`;
- no task acquired an instruction, intrinsic, operand, or call blocker;
- the builder therefore preserves the frozen 225-task subset, including the previously
  measured 156 baseline TIMEOUT tasks.

| IR metric over 225 built tasks | Minimum | Median | Maximum | Sum |
|---|---:|---:|---:|---:|
| functions | 3 | 4 | 5 | 932 |
| blocks | 7 | 184 | 519 | 38,099 |
| integer/Boolean SSA values | 53 | 506 | 1,325 | 105,309 |
| event sites | 42 | 282 | 720 | 60,414 |

The IR is still construction evidence, not a verification result. Guard activation,
RF/CO constraints, CAT checking, model materialization, and interpreter replay remain
required before this lane can change any verdict or terminal classification.

## Strict-admission rerun D

After the first executable encoder exposed additional semantic boundaries, admission was
tightened to reject integer function arguments, ordinary direct calls, joins, unresolved
or reused thread entries, dynamic mutex addresses, non-integer memory accesses, unsupported
terminators such as `switch`, and every GEP/alloca address until byte offsets and lifetime
are modeled. Census D is the authoritative result for this stricter implementation:
`server-results/finite-skeleton-pthread-wmm-20260718d/`.

- Docker/BenchExec manifest status: 0;
- complete structural records: 283/283;
- complete IR records: 283/283;
- built: 225/283;
- fail-open: 58/283, all and only `dynamic-memory-address`;
- built totals remain exactly 932 functions, 38,099 blocks, 105,309 values, and 60,414
  event sites.

Thus no newly rejected semantic category occurs in the existing 225-task target subset;
the strict gates close unsound cases without reducing measured `pthread-wmm` coverage.
This is admission/IR evidence only, not a terminal-performance or correctness result.
