# Preventive-history census

## Conclusion

Decision: **prototype suppression of adaptive-offline preventive retention**. The archive contains 87 completed CAT-stat logs.
Preventive synchronization retained 307,832 checkpoints; 0 distinct
checkpoints were later restored (0.000%).

Adaptive-offline preventive retention created 307,753 checkpoints and later used
0 (0.000%). Never-used
retentions computed from lifetime accounting are 307,832; the identity
`retentions - unique_used == unused_discards + unused_live` is
True.

Summed per-task maximum preventive history is 73,732,376 of 92,023,544
base bytes (80.12%).
