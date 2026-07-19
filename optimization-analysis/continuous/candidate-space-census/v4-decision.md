# Generic preventive V4 decision

V4 selects one external-RF structural certificate and keeps lazy sparse focus
reachability. Four repetitions consistently convert four TIMEOUTs (three TRUE and one
FALSE), with no OOM and no safe execution-count mismatch. It prunes
14,187,204/18,058,008 candidates (78.56%).

The representation remains too expensive: CPU ratio is 1.05457 with 95% CI
[0.98566, 1.15555], wall ratio 1.04833 [0.98019, 1.14685], and RSS ratio 1.03439
[1.00030, 1.08789]. V4 is not retained as the final implementation.

V5 keeps the same name-independent structural selection and exact pruning theorem, but
uses the selected evaluator order plus its dense transitive closure. Earlier P0.3 data
shows one such order fits the memory limit and is faster; the OOM arose only when V1
materialized two checked orders. V5 tests whether that representation result transfers
to the generic certificate.
