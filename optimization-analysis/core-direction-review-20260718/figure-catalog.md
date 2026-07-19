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
