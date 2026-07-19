# V12 complete 725-task analysis

V12 is search-preserving, so execution/search-count equality is required; this criterion does not apply unchanged to coarser quotient experiments.

- Tasks: 725 per variant; total rows: 1,450.
- Status/category differences: 0; common-correct complete-execution differences: 0; common-correct search-counter differences: 0.
- Coverage regressions (before completed, after resource-failed): 0; reverse transitions: 0.
- Status counts: before `{'true': 289, 'false(unreach-call)': 112, 'TIMEOUT': 276, 'OUT OF MEMORY': 22, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`; after `{'true': 289, 'false(unreach-call)': 112, 'TIMEOUT': 276, 'OUT OF MEMORY': 22, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`.
- CAT counter totals: `{'before': {'preventive-direct-checks': 1205405, 'preventive-direct-base-candidates': 16119624922, 'preventive-direct-edge-candidates': 2943514438, 'preventive-rf-candidates': 10197044, 'preventive-rf-pruned': 5814007, 'preventive-co-candidates': 13517111, 'preventive-co-pruned': 12899587}, 'after': {'preventive-direct-checks': 1205405, 'preventive-direct-base-candidates': 16119624922, 'preventive-direct-edge-candidates': 2943514438, 'preventive-rf-candidates': 10197044, 'preventive-rf-pruned': 5814007, 'preventive-co-candidates': 13517111, 'preventive-co-pruned': 12899587}}`.
- CPU after/before: 0.988710 [0.977375, 1.001604].
- Wall after/before: 0.989603 [0.978159, 1.002127].
- RSS after/before: 1.000179 [0.999909, 1.000487].
