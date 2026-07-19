# Generic preventive V1: reject full-closure implementation

Two complete 96-task PSO before/after repetitions produced identical outcome counts.
Each baseline had 57 TRUE, 27 FALSE and 12 TIMEOUT. Each candidate had 59 TRUE,
28 FALSE, 4 TIMEOUT and 5 OOM. Thus three tasks became terminal and eight TIMEOUTs
disappeared, but five were converted into OOM under the same 4 GB limit.

The cause is implementation-level, not a weakening of the pruning theorem: V1
materialized a dense transitive closure for every structurally certified acyclic check
(both coherence and PSO order). This duplicates O(V^2) state at every candidate point.
V1 is rejected as a time-for-space trade and the remaining two planned repetitions were
stopped after the first two repeats agreed exactly.

V2 retains the same structural certificate and exact reversal theorem but computes only
the forward and reverse reachable sets around the newly added read/write. A temporary
inverse CSR is built and discarded per checked order, reducing auxiliary storage to
O(V+E) while answering every reach query used by RF/CO pruning exactly.

Four V2 repetitions still produced five OOMs. The remaining cause was the preventive
constructor disabling lazy-cycle evaluation so the checked order itself was published as
a dense O(V^2) value. V3 keeps lazy-cycle streaming enabled and materializes an exact
sparse relation from its already certified edge interpreter only for focus reachability.
