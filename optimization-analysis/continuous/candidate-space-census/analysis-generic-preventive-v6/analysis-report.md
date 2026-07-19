# P0.3 preventive-order pruning

## Conclusion

Decision under the frozen gate: **reject**. CPU pruning/baseline is
1.04146 with task-bootstrap 95% CI
[0.97512, 1.13572]. RSS is
1.03698 [1.00010,
1.09524].

The matrix has 768 cells, 16 resource-sensitive status
differences, and 0 safe execution-count mismatches. Completed
pruning logs removed 14,187,204/18,058,008 offered RF/CO candidates
(78.56%), with 0
all-pruned safety fallbacks.

On 336 cells with final accounting on both sides, GenMC work popped
changes from 1,628,696 to 423,932
(73.97% reduction). RF/CO choices queued change from
1,812,404 to 330,988
(81.74% reduction), and maximum retained work changes from
51,601 to 801. These counters distinguish
candidate-space reduction from consistency-query avoidance.
