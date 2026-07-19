# V10 complete 725-task analysis

- Tasks: 725 per variant; total rows: 1,450.
- Status/category differences: 5; complete-execution differences: 4; common-correct search-counter differences: 0.
- Status counts: V9 `{'true': 289, 'false(unreach-call)': 112, 'TIMEOUT': 276, 'OUT OF MEMORY': 22, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`; V10 `{'true': 289, 'false(unreach-call)': 107, 'TIMEOUT': 280, 'OUT OF MEMORY': 22, 'ERROR (compilation)': 13, 'ERROR (unsupported property: invalid memory access)': 7, 'ERROR (unsupported external)': 2, 'ABORTED': 2, 'SEGMENTATION FAULT': 1, 'TIMEOUT (false(unreach-call))': 1, 'ERROR (unsupported property: uninitialized memory)': 1}`.
- V10 tasks with conflict-core hits: 158/725.
- Conflict-core totals: `{'conflict-core-learn-attempts': 259523, 'conflict-core-learned': 68812, 'conflict-core-unsupported': 169156, 'conflict-core-duplicate-subsumed': 21555, 'conflict-core-evicted': 7112, 'conflict-core-match-queries': 18299835, 'conflict-core-literal-checks': 8600686460, 'conflict-core-hits': 14193035, 'conflict-core-direct-checks-avoided': 0, 'conflict-core-rf-pruned': 4383344, 'conflict-core-co-pruned': 9809691, 'conflict-core-max-clauses': 61700, 'conflict-core-max-literals': 1960748, 'conflict-core-max-bytes': 31371968}`.
- CPU V10/V9: 1.049916 [1.025769, 1.077275].
- Wall V10/V9: 1.048947 [1.024901, 1.076183].
- RSS V10/V9: 1.000341 [0.999941, 1.000849].
