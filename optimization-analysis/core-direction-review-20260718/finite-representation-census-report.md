# pthread-wmm finite representation census

## 结论

Yogar 式 scheduling abstraction 值得保留，但不能单独作为下一版原型。实际 225 个
admitted `pthread-wmm` 骨架中，当前 Z3 公式的 RF pairwise at-most-one 规模约为 CO pair
规模的 7.5 倍；在 156 个 baseline TIMEOUT 子集中也是 7.4 倍。只移除 CO rank/pair，
仍会留下占主导的 RF 二次编码。

下一版无 verdict 原型应同时做到：

1. 首次 query 不创建 CO rank、`co_before` 和 scheduling completion；
2. RF choice 保持完整，但把每个 read 的 `C(k,2)` pairwise at-most-one 改为线性或 solver
   原生 cardinality；
3. 先只测首个 abstract error model 的时间/RSS/solver counters；
4. 只有首模显著改善后，才实现 EOG/ordering completion 和 kernel-reason refinement。

这不是细枝末节的 clause 调参，而是由实际公式规模确定的两项核心表示变化。

## 实验协议

- 数据集：实际 `pthread-wmm` 全部 283 个 YAML；
- 二进制：服务器 Docker 内 GCC 13 Release，Z3 enabled；
- 模式：`--finite-skeleton-stats-only`，不调用 solver、不探索、不产生 verdict；
- 限制：15 CPU s、4 GiB、每任务 1 core、48 个并行 worker；
- 结果目录：
  `server-results/finite-representation-pthread-wmm-20260718a/`；
- manifest status：0；283/283 均有日志；
- 开销：总 CPU 21.255 s，单任务最大 wall 0.113 s，最大 RSS 27,099,136 B；
- 225 个建成骨架，58 个按既有 dynamic-address admission fail-open；覆盖与 census D
  完全一致。

BenchExec 将 stats-only 的返回标为 283 个 `ERROR`，因为它没有输出 SV-COMP property
verdict。这不是程序失败：283 个 return value 均为 0，manifest 为 0，且所有目标统计均
来自日志。该批次只作为 representation evidence，不能计作正确性或性能 verdict matrix。

## 主要结果

| cohort / metric | p50 | p90 | p95 | p99 | max | sum |
|---|---:|---:|---:|---:|---:|---:|
| all 225 RF selectors | 1,150 | 1,555 | 2,378 | 2,960 | 3,906 | 256,010 |
| all 225 RF pairs | 4,355 | 7,713 | 8,321 | 11,404 | 18,624 | 1,059,950 |
| all 225 CO pairs | 590 | 927 | 944 | 1,038 | 1,088 | 141,179 |
| all 225 CO rank bits | 289 | 364 | 366 | 486 | 530 | 62,366 |
| all 225 potential FR | 14,254 | 25,124 | 25,134 | 32,510 | 43,404 | 3,363,650 |
| TIMEOUT 156 RF pairs | 5,436 | 7,714 | 7,784 | 13,211 | 18,624 | 824,229 |
| TIMEOUT 156 CO pairs | 735 | 943 | 944 | 1,039 | 1,088 | 111,979 |
| TIMEOUT 156 value bits | 4,890 | 5,627 | 6,733 | 11,565 | 12,765 | 687,459 |
| non-TIMEOUT 69 RF pairs | 3,088 | 6,463 | 8,522 | 9,356 | 9,356 | 235,721 |
| non-TIMEOUT 69 CO pairs | 403 | 602 | 621 | 690 | 690 | 29,200 |
| non-TIMEOUT 69 value bits | 4,728 | 7,613 | 8,245 | 11,307 | 11,307 | 303,176 |

所有 225 个骨架的 `rf-reads-without-source` 均为 0。最大合法 RF source 数是 22，最大
单地址 write 数是 42，因此 RF/CO 热点不是由错误 admission 或极端无限结构造成，而是
该数据集普遍存在的有限但宽的 interference choice。

## 对论文迁移的修正

PLDI/TOPLAS/Deagle 通过 on-demand FR 获得的收益不能直接外推到当前 finite lane：我们的
Z3 encoder 从未预编码 FR，FR 已在完整 assignment 物化后由通用 CAT evaluator 计算。
当前真正 eager 的并发约束是：

- RF selectors 及 activation/value implications；
- 每个 read 的 pairwise at-most-one；
- 每地址 CO BV ranks、pairwise distinct 和 `co_before` definitions。

因此优先级应是：

- P1a：Yogar initial abstraction + linear/native RF cardinality，只测首模；
- P1b：结构认证 SC 的 exact EOG/ordering completion；
- P1c：kernel-reason refinement，无法证明时 exact abstract-model blocker；
- P2：TOPLAS TSO/PSO extension；
- P3：PPoPP interference decision priority；
- P4：经独立审计后才考虑 preventive propagation。

## 正确性边界

- census 只读 `Program`，不构造 `Solver`，不改变默认执行；
- 新增单元覆盖 RF source、pair、CO rank/bit/pair、PO 和 FR 上限计数；
- 本地 GCC/Clang 等价构建路径中 focused unit 5/5、finite CTest 3/3 通过；
- 服务器 Docker Release 同样是 unit 5/5、finite CTest 3/3；
- 下一原型仍只允许 replay-confirmed FALSE，任何 UNSAT/unknown/budget 均回退 native；
- EOG completion 未实现前，abstract model 只能用于计时，不能触发 replay 或 verdict。

## 原始与可复现材料

- benchmark definition：`finite-representation-census-pthread-wmm.xml`；
- launcher：`launch-finite-representation-census-pthread-wmm.sh`；
- analysis：`analyze_finite_representation_census.py`；
- raw result、283 logs、manifest 和 hashes：
  `server-results/finite-representation-pthread-wmm-20260718a/`。
