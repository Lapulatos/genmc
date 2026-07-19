# V11 complete 725-task analysis

- Tasks: 725 per variant; total rows: 1,450.
- Status/category differences: 25; common-correct complete-execution differences: 0; common-correct search-counter differences: 0.
- Coverage regressions (V9 completed, V11 resource-failed): 18; reverse transitions: 0.
- Status counts: V9 `{'true': 289, 'false(unreach-call)': 112, 'TIMEOUT': 276, 'OUT OF MEMORY': 22, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`; V11 `{'true': 281, 'false(unreach-call)': 102, 'TIMEOUT': 289, 'OUT OF MEMORY': 27, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`.
- V11 tasks with successful focus reach: 216/725.
- V9/V11 focus and root totals: `{'v9': {'focus-reach-attempts': 0, 'focus-reach-successes': 0, 'focus-reach-fallbacks': 0, 'focus-reach-edge-candidates': 0, 'preventive-direct-checks': 1205405, 'preventive-direct-edge-candidates': 2943514438, 'preventive-rf-candidates': 10197044, 'preventive-rf-pruned': 5814007, 'preventive-co-candidates': 13517111, 'preventive-co-pruned': 12899587}, 'v11': {'focus-reach-attempts': 571899, 'focus-reach-successes': 571899, 'focus-reach-fallbacks': 0, 'focus-reach-edge-candidates': 489083393, 'preventive-direct-checks': 0, 'preventive-direct-edge-candidates': 0, 'preventive-rf-candidates': 4115361, 'preventive-rf-pruned': 2311815, 'preventive-co-candidates': 4906007, 'preventive-co-pruned': 4658542}}`.
- CPU V11/V9: 1.108462 [1.067238, 1.154924].
- Wall V11/V9: 1.111303 [1.069816, 1.156717].
- RSS V11/V9: 0.999895 [0.999670, 1.000116].
