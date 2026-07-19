# pthread-wmm 固定 15 例迭代面板

## 用途

后续工具实现中的每轮性能实验先跑固定 15 例，不再反复跑完整 283 个 `pthread-wmm`。
面板通过后才运行全包。固定任务和基线数据见 `pthread-wmm-15-panel.tsv`。

这是完善工具阶段的默认截断策略：每轮取难、中、易各 5 个。它只缩短迭代性能实验，
不替代单元测试、sanitizer、差分检查、CAT 复核和 native replay 等正确性/完备性门。

## 分类原则

- 易：baseline 小于 0.25 s；3 个 FALSE、2 个 TRUE。`rfi005.yml` 特意保留为“公式较大
  但错误极易发现”的对照，避免把求解规模和原生找错时间混为一谈。
- 中：baseline 10--50 s；3 个 FALSE、2 个 TRUE，同时覆盖高 RF/SSA 规模和较低公式
  规模但原生探索较慢的安全任务。
- 难：baseline 全部 TIMEOUT，从 admitted TIMEOUT 中按 `RF pairs + CO pairs` 取最大的
  5 个，直接检验首次 symbolic query 和并发表示改造。

## 固定任务

| class | task | baseline | CPU s | RF pairs | CO pairs | value bits |
|---|---|---:|---:|---:|---:|---:|
| 易 | `rfi005.yml` | FALSE | 0.081 | 6,404 | 515 | 7,161 |
| 易 | `safe029_power.oepc_pso.oepc_rmo.oepc.yml` | FALSE | 0.107 | 2,057 | 278 | 3,991 |
| 易 | `mix054_power.oepc_power.opt_pso.oepc_rmo.oepc.yml` | FALSE | 0.119 | 2,016 | 273 | 4,023 |
| 易 | `safe029_tso.yml` | TRUE | 0.223 | 743 | 234 | 329 |
| 易 | `safe010_tso.yml` | TRUE | 0.227 | 742 | 233 | 361 |
| 中 | `safe004_power.oepc_pso.oepc_rmo.oepc.yml` | FALSE | 13.185 | 8,613 | 602 | 8,285 |
| 中 | `mix024_power.oepc_pso.oepc_rmo.oepc_tso.oepc_tso.opt.yml` | FALSE | 26.192 | 5,043 | 690 | 4,890 |
| 中 | `mix035.oepc.yml` | FALSE | 49.522 | 4,561 | 620 | 4,857 |
| 中 | `rfi002_tso.yml` | TRUE | 10.240 | 9,356 | 579 | 11,307 |
| 中 | `safe017_tso.yml` | TRUE | 49.834 | 1,506 | 383 | 458 |
| 难 | `safe035_power.yml` | TIMEOUT | 60.751 | 18,624 | 899 | 10,954 |
| 难 | `mix023_tso.yml` | TIMEOUT | 60.977 | 13,211 | 1,088 | 12,765 |
| 难 | `mix014_tso.oepc.yml` | TIMEOUT | 60.888 | 11,404 | 1,038 | 11,051 |
| 难 | `mix008_tso.oepc.yml` | TIMEOUT | 60.981 | 10,910 | 1,039 | 11,565 |
| 难 | `safe023_power.oepc_power.opt_pso.oepc_pso.opt_rmo.oepc_rmo.opt.yml` | TIMEOUT | 60.982 | 10,811 | 692 | 8,917 |

## 每轮通过条件

1. 正确性门先通过；15 例 baseline terminal 必须与冻结记录一致。
2. diagnostic-only 原型不能改变 verdict；记录 first-model time、solver status、CPU、wall、
   RSS、variables/constraints、decisions/conflicts/propagations。
3. verdict-changing FALSE lane 只能接受 native replay-confirmed error；TRUE/UNSAT/unknown
   均走原生 fallback。
4. 难例至少有 3/5 在既定截断时间内产生首个 abstract model，且中/易例没有系统性回退，
   才进入完整 283 包级测试。
5. 完整 283 只有 TIMEOUT/OOM 减少、正确 terminal 不下降且无 unsupported/ABORTED，才进入
   725 全量。

面板用于迭代，不替代最终包级正确性与收益结论。
