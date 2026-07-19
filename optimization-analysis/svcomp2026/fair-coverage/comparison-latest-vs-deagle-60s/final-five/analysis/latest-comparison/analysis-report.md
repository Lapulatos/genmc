# Latest CAAT census comparison

- Dataset: the same 725 adapted C.Concurrency tasks.
- Limits: 60 CPU seconds, 4 GB, one core per task, one run per method.
- Ratios use only task pairs where both methods terminate with the same true/false verdict.
- TSO/PSO coverage counts are descriptive because the task labels are SC competition labels.

## Coverage

- GenMC: correct=408, wrong=31, terminal=439, TIMEOUT=210, OOM=50, other unknown=26.
- GenMC+CAAT-SC: correct=398, wrong=30, terminal=428, TIMEOUT=240, OOM=31, other unknown=26.
- GenMC+CAAT-TSO: correct=385, wrong=30, terminal=415, TIMEOUT=253, OOM=31, other unknown=26.
- GenMC+CAAT-PSO: correct=355, wrong=29, terminal=384, TIMEOUT=274, OOM=41, other unknown=26.
- Deagle: correct=615, wrong=3, terminal=618, TIMEOUT=16, OOM=10, other unknown=81.
- Previous GenMC+CAAT-SC: correct=388, wrong=30, terminal=418, TIMEOUT=224, OOM=57, other unknown=26.

## Same-verdict paired cost ratios

- GenMC / Deagle: n=375, CPU=0.171327 [0.132324, 0.22447], RSS=0.987891 [0.893088, 1.08997].
- GenMC+CAAT-SC / Deagle: n=367, CPU=0.159379 [0.121254, 0.210235], RSS=0.979084 [0.882976, 1.08267].
- GenMC+CAAT-TSO / Deagle: n=359, CPU=0.144266 [0.110853, 0.188813], RSS=0.966747 [0.868852, 1.07093].
- GenMC+CAAT-PSO / Deagle: n=330, CPU=0.122751 [0.0933579, 0.162152], RSS=0.918423 [0.821525, 1.02393].
- Latest GenMC+CAAT-SC / Previous GenMC+CAAT-SC: n=418, CPU=0.915446 [0.894591, 0.934561], RSS=0.994343 [0.986828, 1.00024].
- Current GenMC / Previous GenMC: n=439, CPU=0.982636 [0.976761, 0.988256], RSS=1.00017 [0.999979, 1.00035].
