# Generic preventive V3 decision

V3 is semantically clean and eliminates the dense-order OOM, but is not retained as the
default implementation. Across four 96-task PSO repetitions it converted the same three
TIMEOUT tasks to correct TRUE in every repeat, with zero OOM and zero safe execution-count
mismatch. It pruned 13,465,988 of 16,175,796 offered RF/CO candidates (83.25%).

For the 84 tasks correct in both variants, CPU candidate/baseline is 1.06106 with
task-bootstrap 95% CI [0.99015, 1.16826], wall time is 1.05859
[0.98864, 1.16341], and RSS is 1.03378 [0.99985, 1.08709]. The coverage gain is real,
but replaying both certified lazy edge programs at 1,160,904 prefix queries makes the
common completed workload slower and larger.

V4 retains exactly one sufficient acyclicity certificate. Selection is structural and
fail-closed: prefer an external-RF seed to an all-RF seed, with source check order as the
stable tie breaker. Removing a certificate can only lose pruning opportunities; it cannot
remove a consistent execution. The expected benefit is roughly halving lazy relation and
inverse-CSR construction while preserving the PSO-order rejection theorem.
