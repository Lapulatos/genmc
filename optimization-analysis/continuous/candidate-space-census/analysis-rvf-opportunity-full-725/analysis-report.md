# Candidate-space census

Rows: 2175; accounting violations: 0.

## SC

- completed verdict: 428/725; counter-bearing logs: 640/725
- statuses: ABORTED=2, ERROR (compilation)=13, ERROR (unsupported external)=2, ERROR (unsupported property: invalid memory access)=7, ERROR (unsupported property: uninitialized memory)=1, OUT OF MEMORY=32, SEGMENTATION FAULT=1, TIMEOUT=239, false(unreach-call)=130, true=298
- rf_offered: total=67978223, median=39, p95=426561, max=6499992
- rf_queued: total=13087009, median=11, p95=120649, max=159107
- rf_value_choice_points: total=8825258, median=11, p95=74155, max=108074
- rf_value_classes: total=9085885, median=11.5, p95=83523, max=180533
- rf_same_value_candidate_upper_bound: total=12868426, median=6.5, p95=119888, max=159107
- rf_same_value_choice_points: total=8673356, median=6.5, p95=74155, max=108074
- max_rf_same_value_class: total=3602, median=2, p95=9, max=801
- co_offered: total=73759533, median=54, p95=674210, max=894190
- co_queued: total=524131, median=0, p95=0, max=513956
- backward_offered: total=16508576, median=11, p95=119881, max=3770191
- backward_queued: total=12639639, median=11, p95=108268, max=159087
- work_added: total=26250931, median=23, p95=239769, max=652442
- work_popped: total=25952353, median=12.5, p95=239752, max=652400
- max_retained_work: total=305481, median=9, p95=54, max=149201
- validity_queries: total=27121632, median=12, p95=250549, max=669716
- realized_revisit_prefixes: total=22090308, median=12.5, p95=203394, max=652399
- inconsistent_revisit_prefixes: total=207, median=0, p95=0, max=161
- optimistic same-value RF candidate upper bound: 12868426/67978223 = 18.93%
- RF choice points with a repeated value class: 8673356/8825258 = 98.28%
- post-generation rejection fraction: 207/22090308 = 0.00%
- completed verdict same-value bound: 1086942/4472513 = 24.30%; logs=428/428
- TIMEOUT same-value bound: 11781470/39705672 = 29.67%; logs=200/239
- OOM same-value bound: 5/23799961 = 0.00%; logs=4/32
- largest TIMEOUT same-value opportunities:
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe030_tso.yml: 159107/519227 = 30.64%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe028_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 152753/498258 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe020_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 152753/498258 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe011_tso.yml: 151997/495669 = 30.67%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe004_tso.yml: 151997/495669 = 30.67%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe021_tso.yml: 151997/495669 = 30.67%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread/queue_longer.yml: 149200/299094 = 49.88%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe031_tso.yml: 144788/472214 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe015_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 144788/472214 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe003_tso.yml: 144788/472214 = 30.66%

## TSO

- completed verdict: 415/725; counter-bearing logs: 636/725
- statuses: ABORTED=2, ERROR (compilation)=14, ERROR (unsupported external)=2, ERROR (unsupported property: invalid memory access)=7, ERROR (unsupported property: uninitialized memory)=1, OUT OF MEMORY=31, SEGMENTATION FAULT=1, TIMEOUT=252, false(unreach-call)=121, true=294
- rf_offered: total=57718147, median=39, p95=332325, max=6499992
- rf_queued: total=9977479, median=11.5, p95=90982, max=131477
- rf_value_choice_points: total=6788809, median=11, p95=64215, max=85614
- rf_value_classes: total=7031351, median=12, p95=64215, max=180533
- rf_same_value_candidate_upper_bound: total=9775191, median=7, p95=90982, max=131478
- rf_same_value_choice_points: total=6642781, median=7, p95=59302, max=85614
- max_rf_same_value_class: total=3568, median=2, p95=9, max=801
- co_offered: total=56397273, median=54, p95=518564, max=724967
- co_queued: total=384750, median=0, p95=0, max=374575
- backward_offered: total=12531769, median=11, p95=90966, max=2747934
- backward_queued: total=9681139, median=11, p95=86640, max=131455
- work_added: total=20043511, median=24, p95=181948, max=475457
- work_popped: total=19868847, median=13, p95=181921, max=475420
- max_retained_work: total=181106, median=10, p95=53, max=99601
- validity_queries: total=20824421, median=12, p95=192622, max=488041
- realized_revisit_prefixes: total=16927328, median=13, p95=154832, max=475420
- inconsistent_revisit_prefixes: total=207, median=0, p95=0, max=161
- optimistic same-value RF candidate upper bound: 9775191/57718147 = 16.94%
- RF choice points with a repeated value class: 6642781/6788809 = 97.85%
- post-generation rejection fraction: 207/16927328 = 0.00%
- completed verdict same-value bound: 371893/2099540 = 17.71%; logs=415/415
- TIMEOUT same-value bound: 9403284/31818569 = 29.55%; logs=209/252
- OOM same-value bound: 5/23799961 = 0.00%; logs=4/31
- largest TIMEOUT same-value opportunities:
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe009_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 131478/428238 = 30.70%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe018_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 130876/427182 = 30.64%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe017_tso.yml: 130876/427182 = 30.64%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe028_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 123535/402933 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe020_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 123535/402933 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe026_tso.yml: 123534/402931 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe030_tso.yml: 122974/401051 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe011_tso.yml: 122974/401051 = 30.66%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe031_tso.yml: 115676/377408 = 30.65%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe004_tso.yml: 115676/377408 = 30.65%

## PSO

- completed verdict: 401/725; counter-bearing logs: 607/725
- statuses: ABORTED=2, ERROR (compilation)=13, ERROR (unsupported external)=2, ERROR (unsupported property: invalid memory access)=7, ERROR (unsupported property: uninitialized memory)=1, OUT OF MEMORY=22, SEGMENTATION FAULT=1, TIMEOUT=276, false(unreach-call)=112, true=289
- rf_offered: total=23005964, median=39, p95=117662, max=6499992
- rf_queued: total=2947392, median=8, p95=35374, max=76012
- rf_value_choice_points: total=2022363, median=7, p95=20009, max=51389
- rf_value_classes: total=2182054, median=10, p95=24334, max=127401
- rf_same_value_candidate_upper_bound: total=2787706, median=5, p95=29029, max=54374
- rf_same_value_choice_points: total=1922863, median=4, p95=19559, max=29978
- max_rf_same_value_class: total=2096, median=2, p95=8, max=64
- co_offered: total=16320511, median=50, p95=163694, max=420522
- co_queued: total=332814, median=0, p95=0, max=322765
- backward_offered: total=5269407, median=8, p95=35360, max=2362695
- backward_queued: total=2811980, median=8, p95=29011, max=53592
- work_added: total=6092320, median=17, p95=70734, max=409678
- work_popped: total=6068682, median=10, p95=70710, max=409635
- max_retained_work: total=28472, median=8, p95=46, max=16388
- validity_queries: total=6465873, median=12, p95=72982, max=420509
- realized_revisit_prefixes: total=5243489, median=10, p95=59684, max=409634
- inconsistent_revisit_prefixes: total=67, median=0, p95=0, max=66
- optimistic same-value RF candidate upper bound: 2787706/23005964 = 12.12%
- RF choice points with a repeated value class: 1922863/2022363 = 95.08%
- post-generation rejection fraction: 67/5243489 = 0.00%
- completed verdict same-value bound: 184219/861285 = 21.39%; logs=401/401
- TIMEOUT same-value bound: 2603476/9144620 = 28.47%; logs=196/276
- OOM same-value bound: 1/12999981 = 0.00%; logs=2/22
- largest TIMEOUT same-value opportunities:
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-theta/exponential-4.yml: 54374/86640 = 62.76%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe009_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 43638/141878 = 30.76%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe018_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 43359/141376 = 30.67%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe017_tso.yml: 43359/141376 = 30.67%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe026_tso.yml: 43359/141376 = 30.67%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe030_tso.yml: 42861/141481 = 30.29%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe016_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 36326/118102 = 30.76%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe028_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 35982/117662 = 30.58%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe020_pso.oepc_pso.opt_tso.oepc_tso.opt.yml: 35982/117662 = 30.58%
  - ../../../../../svcomp2026-caat/sv-benchmarks/c/pthread-wmm/safe031_tso.yml: 35764/118010 = 30.31%

