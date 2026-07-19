# P0.1 rejection-kernel census: strict analysis

## Conclusion

只推进面向 PSO/未认证通用 CAT 的小型 P0.2 原型；删除普查实现，不在 SC/TSO 默认启用。PSO 历史 TIMEOUT/OOM 样本中，模拟命中占全部查询
50.61%（10,862/21,463），而
SC/TSO 仅为 0.0134%/0.0140%。
当前普查代码会对每个失败执行完整解释和删除最小化，不能作为生产实现保留。

## Formal overhead

96 任务、SC/TSO/PSO、三次成对重复共 1,728 个运行单元。
以每个任务跨模型聚合为主要分析单位，CPU census/baseline 几何均值比为
1.20162，task-bootstrap 95% CI
[1.12795, 1.29234]。
状态差异 27 个，安全任务完整执行数差异
0 个；资源边界扰动单独报告，不能解释为语义错误。

## Mechanism

- PSO 大任务：11,862 个 rejected query，10,862 次命中，
  21,360 个 exact-duplicate explanation，0 个严格 subsumption。
- 删除最小化仅把 199,136 个原始文字降到 198,362，
  不足以抵消解释/最小化成本。
- SC/TSO 大任务绝大多数查询本身一致，拒绝核无法减少有效执行爆炸；多数 OOM 在第一条
  consistency checkpoint 前发生，必须由状态压缩、property/dependence CEGAR 或符号前端处理。

## Claim candidates

- Claim: exact positive rejection reuse is frequent for the generic PSO path.
  - Source evidence: `stats-strict.json`, TIMEOUT/OOM PSO counters.
  - Allowed wording: the census establishes a bounded P0.2 prototype opportunity.
  - Forbidden stronger wording: rejection kernels solve GenMC's broad TIMEOUT/OOM gap.
  - Uncertainty: the census itself is expensive and changes resource-limit outcomes.
  - Next check: raw-kernel lookup before evaluator synchronization, one worker, exact oracle.
  - Decision: keep as a prototype hypothesis.

- Claim: a universal SC/TSO/PSO blocker is justified.
  - Source evidence: SC/TSO hit/all-query rates below 0.02%.
  - Allowed wording: none.
  - Forbidden stronger wording: enable blocker learning for every model by default.
  - Uncertainty: other CAT models may differ.
  - Next check: model-fragment/profile-specific activation only.
  - Decision: discard.

## Limits

The 60-task resource cohort has one repetition and intentionally censored TIMEOUT/OOM logs; it supports
mechanism and coverage claims, not inferential performance claims. The 96-task overhead matrix has three
repetitions, and inference clusters by task rather than treating repetitions or model cells as independent.
