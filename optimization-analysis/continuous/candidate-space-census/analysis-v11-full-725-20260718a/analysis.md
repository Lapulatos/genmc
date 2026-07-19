# V11 complete 725-task analysis

- Tasks: 725 per variant; total rows: 1,450.
- Status/category differences: 16; complete-execution differences: 0; common-correct search-counter differences: 31.
- Status counts: V9 `{'true': 289, 'false(unreach-call)': 112, 'TIMEOUT': 276, 'OUT OF MEMORY': 22, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`; V11 `{'true': 292, 'false(unreach-call)': 118, 'TIMEOUT': 260, 'OUT OF MEMORY': 29, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`.
- V11 tasks with successful focus reach: 243/725.
- V9/V11 focus and root totals: `{'v9': {'focus-reach-attempts': 0, 'focus-reach-successes': 0, 'focus-reach-fallbacks': 0, 'focus-reach-edge-candidates': 0, 'preventive-direct-checks': 1205405, 'preventive-direct-edge-candidates': 2943514438, 'preventive-rf-candidates': 10197044, 'preventive-rf-pruned': 5814007, 'preventive-co-candidates': 13517111, 'preventive-co-pruned': 12899587}, 'v11': {'focus-reach-attempts': 2439449, 'focus-reach-successes': 2439449, 'focus-reach-fallbacks': 0, 'focus-reach-edge-candidates': 965460805, 'preventive-direct-checks': 0, 'preventive-direct-edge-candidates': 0, 'preventive-rf-candidates': 24841290, 'preventive-rf-pruned': 15021469, 'preventive-co-candidates': 24167675, 'preventive-co-pruned': 23119093}}`.
- CPU V11/V9: 0.863836 [0.842023, 0.885209].
- Wall V11/V9: 0.870608 [0.848928, 0.892677].
- RSS V11/V9: 0.994448 [0.987355, 0.999985].
