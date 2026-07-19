# 统计附录

## 数据与 cohort

输入表：
`optimization-analysis/svcomp2026/fair-coverage/comparison-latest-vs-deagle-60s/all-six/html/adapted-725-previous-latest-sc-tso-pso-genmc-deagle-60s.table.csv`

主 cohort 的确定规则是：当前 CAAT-SC `status == TIMEOUT` 且 Deagle
`category == correct`。这是 725 个已执行任务的完整 census，不是随机样本，因此
报告描述性计数、比例、中位数和 P90，不对 family 比例做无意义的显著性检验。

## 核心计数

| 指标 | 数值 |
|---|---:|
| 全部任务 | 725 |
| CAAT-SC TIMEOUT | 240 |
| 原生 GenMC TIMEOUT | 210 |
| Deagle correct | 615 |
| 主 cohort | 200 |
| cohort 中原生 GenMC TIMEOUT | 192 |
| cohort 中含 nondet | 190 |
| Deagle fallback-bound TRUE | 11 |
| 严格语义可比且结论完备 | 9 |

## 资源分布

| 指标 | median | P90 | max |
|---|---:|---:|---:|
| CAAT-SC peak RSS (MB) | 26.284 | 26.543 | 111.833 |
| Deagle CPU (s) | 0.279 | 0.923 | 54.803 |

CAAT-SC RSS 低于 64 MB 的任务为 190/200，达到 512 MB 的任务为 0/200。
Deagle CPU 低于 1 秒的任务为 180/200，但其中大多数含 nondet 语义差异，不能把
该分布直接解释为公平 speedup。

## 分类规则

- `has_nondet`：去注释后的源码中存在 `__VERIFIER_nondet_*()` 调用。
- `fallback_bound_3`：Deagle wrapper 日志同时包含无法确定 loop bound 和
  `Unwindset`，随后使用未知循环 bound=3。
- `suggested_finite_bound`：日志包含 `All loops can be statically determined!`。
- `strict_semantics_comparable`：无 nondet，且不是 fallback-bound TRUE。

这里的 strict 只处理已发现的两项主要语义差异，不等于对两个前端的所有 C/LLVM
语义做了形式等价证明。

## 可复现命令

```sh
python3 optimization-analysis/timeout-vs-deagle/analyze_cohort.py
python3 optimization-analysis/timeout-vs-deagle/make_figures.py
```

第一条重建 `primary-cohort.tsv` 和 `summary.json`；第二条只用 Python 标准库重建
三个 SVG。脚本不启动工具、不连接服务器、不修改原始 XML/log/source。

## 主要限制

1. TIMEOUT 日志没有 `--cat-stats`，不能分解 interpreter、revisit、CAT query 的
   实测时间比例。
2. Deagle wrapper 删除了 solver 临时输出，无法从现有 archive 恢复每任务的
   conflicts/decisions/theory propagations。
3. 190 个 nondet 任务输入语义不同，不能报告 paired speedup。
4. 11 个 bounded TRUE 不完备，不能计为严格 proof 差距。
5. 9 个严格任务数量小，当前用于机制定位和原型回归，不用于泛化总体效应量。

