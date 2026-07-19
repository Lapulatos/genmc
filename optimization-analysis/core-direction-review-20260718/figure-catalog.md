# Figure catalog

## Figure 1: mechanism and end-to-end CPU evidence

- Files: `figures/figure-01-mechanism-cpu.pdf` and `.png`.
- Purpose: show that the recent evaluator-only variants do not translate internal-work
  reductions into end-to-end gains, while V9 changes the generated search.
- Data source: `evidence.tsv`, transcribed from the frozen V9--V12 and SC-RVF decisions.
- Encoding: point is candidate/baseline CPU ratio; bar is the source analysis's 95%
  bootstrap CI; green changes the intended search space, gray preserves it.
- Key observation: V10/V11/V11.1 regress, V12 is neutral, and V9 is the only interval
  entirely below one. SC-RVF is green by design but had zero formal activation, so its
  point is framework drift rather than quotient performance.
- Decision implication: require candidate/class/subtree reduction before treating a new
  branch as a core optimization.
- Caveat: protocols and baselines differ; vertical comparison is qualitative mechanism
  synthesis, not a pooled significance test.

## Figure 2: 725-task search funnel

- Files: `figures/figure-02-search-funnel.pdf` and `.png`.
- Purpose: locate the order-of-magnitude gap between pre-generation merge opportunity and
  post-generation inconsistency.
- Data source: `opportunity.tsv`, from the 2,175-row candidate census.
- Encoding: log-scale counts by memory model for RF offers, optimistic same-value
  alternatives, realized revisit prefixes, and inconsistent realized prefixes.
- Key observation: millions of same-value alternatives and realized prefixes coexist with
  only 67--207 post-generation inconsistencies.
- Decision implication: work after candidate realization is structurally too late; merge
  or block choices at generation time.
- Caveat: same-value alternatives are an upper bound and not automatically equivalent.

## Figure 3: clean cumulative P1 common-solved resources

- Files: `figures/figure-03-p1-clean-common-solved.pdf` and `.png`.
- Purpose: show the paired CPU and RSS ratio distributions for all 438 tasks solved by both the
  stable baseline and clean five-item P1 candidate.
- Data source: strict `paired.tsv` under the accepted r3 server result root.
- Encoding: tasks are independently sorted by their candidate/baseline ratio; the dashed line is
  parity and the vertical axis is logarithmic.
- Key observation: most completed tasks improve slightly in CPU and RSS, but the plot intentionally
  excludes nonterminal resource-limit behavior.
- Decision implication: common-solved gains are real workload evidence but are insufficient for
  promotion without the OOM gate.
- Caveat: one paired run measures task heterogeneity, not run-to-run machine variance.

## Figure 4: P1 OOM ablation

- Files: `figures/figure-04-p1-oom-ablation.pdf` and `.png`.
- Purpose: show why the clean cumulative candidate and its main storage component are rejected.
- Data source: the three strict 31-task swapped-NUMA OOM paired result roots listed in
  `p1-clean-full-725-report-20260720.md`.
- Encoding: bar height is summed candidate/baseline CPU; every row remains OOM and both lanes hit
  the same 12-GB task limit.
- Key observation: the cumulative candidate delays OOM by 15.0%, EventDeps alone by 8.19%, and the
  inline-revisit-only difference is 1.40%.
- Decision implication: do not promote P1 storage compression until it changes a memory-limited
  outcome without increasing time.
- Caveat: the inline-only difference is deliberately not pursued because it is below the project's
  practical-effect threshold.

## Figure 5: finite SC RVF exact-control broad gate

- Files: `figures/figure-05-rvf-sc-order-broad-gate.pdf` and `.png`.
- Purpose: present the simultaneous resource and terminal-coverage effect of encoding same-value
  RF classes with an exact SC latest-write order instead of enumerating concrete RF assignments.
- Data source: `finite-rvf-sc-order-evidence.tsv`, frozen from the strict 283-task candidate and
  exact-control BenchExec analyses.
- Encoding: the left panel reports aggregate candidate/control CPU, wall, and summed per-task
  peak-RSS ratios; the right panel stacks SAT, UNSAT, and unknown rows under the fixed task budget.
- Key observation: CPU/wall fall to about 0.67, RSS to 0.323, and terminal coverage rises from 5
  to 107 tasks without a witnessed correctness disagreement.
- Decision implication: retain the linear SC-order formulation on the development branch and use
  it as the baseline for the next order-theory optimization.
- Caveat: timeout-censored totals describe fixed-budget system behavior; this is finite SC only,
  and the figure does not compare production native GenMC performance.
