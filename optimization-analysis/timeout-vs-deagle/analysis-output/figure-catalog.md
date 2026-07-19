# Figure catalog

1. `figures/01-family-composition.svg`：200-task cohort 的 family 与 expected verdict
   构成；显示 `pthread-wmm` 的集中度。
2. `figures/02-evidence-classes.svg`：190 个 nondet 语义差异、1 个仅 bounded-TRUE
   caveat、9 个严格可比任务的互斥分层。
3. `figures/03-runtime-memory-evidence.svg`：Deagle CPU 与 CAAT-SC RSS；显示大多数
   TIMEOUT 保持约 26 MB，属于低内存探索压力，而非 OOM 前兆。

图由 `../make_figures.py` 从 `primary-cohort.tsv` 确定性生成，无随机 jitter、无
统计平滑、无截断任务。
