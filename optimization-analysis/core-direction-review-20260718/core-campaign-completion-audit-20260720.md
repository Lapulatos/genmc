# Core optimization campaign completion audit (2026-07-20)

## Scope

This audit maps the frozen P0/P1/P2 brief to authoritative code, correctness gates, server Docker
artifacts, resource results, and predeclared stop conditions. A direction is considered executed
when it either produces a promoted joint improvement or reaches a correctness/opportunity/resource
gate that forbids further implementation. A rejected or zero-opportunity direction is not reported
as an optimization.

## Server artifact audit

The result roots were reread on 2026-07-20 rather than trusted from report prose. Every compressed
XML was decompressed and counted; every log archive was opened and counted.

| Direction/gate | XML rows per lane | logs per lane | complete marker |
|---|---:|---:|---|
| P0 exact-NA full workload | 725 / 725 | 725 / 725 | yes |
| P0 closed-prefix r2 | 51 | 51 | yes |
| P0 read-kind r3 | 51 | 51 | yes |
| P0 exact-HB CAS r4 | 51 | 51 | yes |
| P1 spin-PHI attribution | 4 / 4 | 4 / 4 | yes |
| P1 spin-PHI OOM cohort | 31 / 31 | 31 / 31 | yes |
| P1 spin-PHI full workload | 725 / 725 | 725 / 725 | yes |
| P2 Gate A1 census | 15 / 15 | 15 / 15 | yes |

## P1: exploration/history memory

### Requirement coverage

- Retained-label, current-graph, stack, worklist, scheduler-cache, CAT snapshot and allocation-stack
  evidence was collected across LibVSync, Weaver, and Goblint tasks.
- Profiling disproved a universal history owner: some Goblint peaks are eager CAT relation state;
  the four LibVSync OOMs are monotonically growing active polling executions with 25.7--29.9
  million labels, zero scheduler-cached labels, and only 2--6 retained work items.
- Exact implicit-empty relations and compact race causality reduced intermediate memory but left
  31/31 OOM and increased CPU by 30.9% and 32.3%; both were removed under the no-tradeoff rule.
- The stable-based five-item layout bundle completed 725-by-2 but increased all-task CPU by 0.43%
  and common-OOM CPU by 15.0% at the same 12-GB cap; it was not promoted.

### Retained result

The constant preheader-seed `SpinAssumePass` rule attacks the actual active-graph cause. In the
full 725 pair it changes correct 413 to 415 and OOM 31 to 29, reduces all-task CPU 0.33%, suite wall
0.43%, and aggregate RSS 4.61%, with zero terminal-verdict or common-terminal execution-count
mismatch. It was promoted as `68e8f56a` to `genmc-caat`.

The next dynamic-seed generalization failed the finite first-iteration completeness oracle before
resource measurement and was removed. No third admission variant is permitted in this line.

## P0: regional SC-RVF

The full 725 exact-NA experiment has zero status differences and zero committed RVF reductions.
Its 51 regional common-terminal tasks are 120.16% slower. The later closed-prefix census observes
63,899 nominal post-frontier loads across 37 tasks but zero supported ordinary-read admissions.
Read-kind and exact-HB audits prove every nominal source belongs to CAS/lock and every causally
valid same-HB class has size one. Therefore the measured production cohort contains no sound
regional quotient opportunity.

Unsafe loop/native re-entry candidates were already rejected by lost-outcome and worker-count
oracles. Implementing the remaining ownership machinery would not create a causally valid class in
this workload, so the activation gate terminates P0 without an optimization claim.

## P2: certified CAT subtree blocking

The stable RF/CO decision and sufficient-certificate contract is frozen in the CDCL design. Gate A1
implements only graph-matched observation state and changes no exploration. Its paired fixed-15
run observes zero installed conflicts, derived cores, mapped choices, non-local conflicts,
recurrence, or potential backjump distance. Resources are neutral and all comparable search
counters are identical.

Full-workload Gate A0 context independently records 14,193,035 V10 positive-core hits, zero extra
direct checks avoided, no parent/all-sibling elimination, and +4.99% CPU. There is consequently no
measured no-consistent-extension clause or work item for a shadow/active clause engine to remove.
P2 correctly stops before adding watched clauses, solver memory, or queue filtering.

## Correctness and claim boundary

- No native checker is used as an SC-order constraint for the SC-RVF/finite lane.
- Execution counts may differ only where the intended equivalence relation changes the partition;
  verdict, error/outcome coverage, replay and completeness oracles remain authoritative.
- TIMEOUT-to-OOM, OOM-to-TIMEOUT, longer survival, local phase wins, cache/core hits, and lower
  timeout-truncated search counters are not counted as successful optimization.
- The 725 paired runs support exact status/resource accounting and descriptive timing. They are not
  presented as repeated-sample statistical significance.

## Repository state at audit

- `genmc-caat`: local and `origin` both `68e8f56aebacb62dc81525f03f00f1590bb47d23`.
- `genmc-caat-opt-dev`: local and `origin` were both
  `cad27201a47b28c96ee8498678b158ceebdfd585` before the closing documentation commit containing
  this audit and the dynamic-seed rejection record.
- Closure action: commit/push these documentation-only dev changes using the confirmed message,
  then verify a clean dev worktree and both remote-tracking SHAs.

## Decision

All three core directions have reached their evidence-defined terminal gate. The only effective new
production result is the constant-seed spin-PHI optimization; P0 and P2, plus the other P1
candidates, remain negative evidence on the development branch. Do not repeat them unless a new
workload demonstrates nonzero causally valid RVF classes, non-local no-extension conflicts, or a
different measured memory owner.
