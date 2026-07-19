# SV-COMP 每工具 BenchExec Python 脚本手册

## 结论

参加 SV-COMP 时，每个工具应有两层 Python 入口，而不是预先修改一份
`sv-benchmarks`：

1. `benchexec/tools/<tool>.py` 是提交到 BenchExec 的 **ToolInfo 模块**，只负责
   定位工具、传递 task/property/data model/resource limit、组装命令、解析结果和
   提取统计。
2. 工具归档中的 `bin/<tool>-svcomp.py` 是 **受测 runner**，在同一个 BenchExec
   run 内读取原始任务、按规则预处理到 `/tmp`、调用实际 verifier、生成 witness，
   并输出机器可解析的 verdict、统计和审计记录。

不能在 ToolInfo 的 `cmdline()` 里写转换后的源码。BenchExec 明确说明 ToolInfo
及其启动的命令运行在独立容器中，那里产生的文件通常不会出现在实际 run 中。
我们当前的 `genmc_svcomp_fair.py` 因为实验使用 `--no-container` 才能工作，它是
研究适配器，不是可直接提交的竞赛模块。

官方依据：

- [SV-COMP 2026 Rules](https://sv-comp.sosy-lab.org/2026/rules.php)
- [SV-COMP 2026 Submission](https://sv-comp.sosy-lab.org/2026/submission.php)
- [BenchExec Tool Integration](https://github.com/sosy-lab/benchexec/blob/main/doc/tool-integration.md)
- [BenchExec BaseTool2 API](https://github.com/sosy-lab/benchexec/blob/main/benchexec/tools/template.py)
- [SV-Benchmarks task format](https://gitlab.com/sosy-lab/benchmarking/sv-benchmarks)

本目录提供两个可复制模板：

- `toolinfo-template/benchexec_toolinfo_template.py`
- `toolinfo-template/svcomp_runner_template.py`

## 1. 两层脚本的责任边界

### 1.1 BenchExec ToolInfo：`benchexec/tools/<tool>.py`

允许做：

- 用 `ToolLocator.find_executable()` 找归档中的 runner；
- `version()` 返回 adapter 与底层工具的真实版本；
- 从 `BaseTool2.Task` 取得输入、property file 和 task options；
- 从 task options 读取 `language`、`ILP32/LP64`；
- 从 `ResourceLimits` 读取 CPU、内存和核数限制并传给 runner；
- `determine_result()` 依据本次 `Run` 的退出状态和结构化输出返回 verdict；
- `get_value_from_output()` 提取执行数、求解统计和转换计数。

禁止或不应做：

- 写转换后的源码或生成本次运行依赖的文件；
- 读取 expected verdict；
- 根据任务路径、文件名、hash 或 category 选择参数；
- 把 `cmdline()` 中保存的“当前 task 状态”用于 `determine_result()`；
- 依赖研究服务器路径、环境变量或工作目录。

BenchExec 只保证一个 run 的 `cmdline()` 先于其 `determine_result()`，不保证结果
紧接着解析，也不保证按 task 顺序解析。判定逻辑必须只依赖 `run.cmdline`、
`run.exit_code`、`run.output`、`run.termination_reason` 和稳定的工具版本信息。

### 1.2 归档 runner：`bin/<tool>-svcomp.py`

runner 是正式受测命令，预处理时间和子进程时间都会计入该任务开销。它负责：

1. 验证单输入 C task、property 和 data model；
2. 只用源码结构和标准外部 API 识别能力需求；
3. 在 `/tmp` 创建任务私有目录；
4. 按严格前置条件应用语义保持转换；
5. 保留 `#line` 到原始文件，供诊断和 witness 定位；
6. 用归档相对路径调用 verifier；
7. 对原始输出做版本固定的精确判定；
8. 输出唯一的 `SVCOMP_RESULT=...`；
9. 输出 `SVCOMP_STAT key=value` 和一行 `SVCOMP_AUDIT=<json>`；
10. 把 witness 留在 run 的工作目录，而不是随 `/tmp` 删除。

## 2. ToolInfo 输入应该怎样使用

### 2.1 输入文件

正式任务给什么就验证什么：`.i` task 使用该 `.i`，`.c` task 使用该 `.c`。
研究阶段“若同名 `.c` 存在就替换 `.i`”提高过兼容覆盖，但竞赛时不能使用未在
task definition 中声明的 sibling 文件，也不能假设评测包中存在它。

如果是 witness-validation task，应使用 BenchExec 的 witness helper 把 witness
从程序输入中分离；普通 verifier task 通常要求 `task.single_input_file`。

### 2.2 property file

必须把 property file 传给 runner。runner 只能针对声明支持的 property 返回
TRUE/FALSE：

- reachability bug -> `false(unreach-call)`；
- data race -> `false(no-data-race)`；
- dereference/free/track/cleanup 分别返回对应 subproperty；
- overflow -> `false(no-overflow)`。

“GenMC 发现非法内存访问”不能在 `unreach-call` task 上自动写成
`false(unreach-call)`；如果没有到达 `reach_error()` 的证据，应返回 UNKNOWN 或
明确 unsupported status。

### 2.3 ILP32/LP64

只能使用 task 的 `data_model`：

- `ILP32` -> 工具真实的 32 位模式；
- `LP64` -> 工具真实的 64 位模式；
- 工具不支持时 -> UNKNOWN/不参加该类别。

TruSt-family 旧 artifact 在我们的实验中只能稳定使用 LP64。把 ILP32 task 强制
按 LP64 编译只适合单独标注的研究表，不能进入正式 SV-COMP 分数表。

### 2.4 resource limits

runner 可以根据 `cpu_cores` 设置工具内部并行上限，但不能扩大 BenchExec 限制。
SV-COMP 2026 的每 run 上限是 4 processing units、15 GB 和 15 分钟 CPU 时间；
未来年份必须重新核对规则，不能把这些数写死在脚本中。

## 3. 动态预处理的竞赛边界

官方允许根据标准外部函数调用（例如 `malloc`、`pthread_create`）识别程序特征并
改变工具行为，但禁止通过程序名、hash、当前 category、注释或普通标识符识别
特定 benchmark。组织方可以重命名标识符并修改/删除注释。

因此预处理规则必须满足：

- detector 基于语法/类型/标准或约定 API，而不是路径白名单；
- precondition 能证明该具体语法形态与目标运行时语义一致；
- rewrite 失败或遇到额外形态时 fail closed；
- 每项规则有独立正例、反例和属性方向测试；
- 不读取 YAML 中的 `expected_verdict`；
- 不根据“历史上这个 task 是 true/false”选择转换。

推荐把每条转换实现成注册项：

```python
Transform(
    name="manual-pthread-create-null-attr",
    precondition=has_exact_supported_signature,
    apply=rewrite_exact_call,
)
```

`apply()` 应再次检查结构数量和类型；不能只依赖 detector 的一次正则命中。

## 4. 已验证的转换经验

下表是已有实验中可迁移的经验。标为“研究限定”的规则不能直接支持正式 TRUE。

| 规则 | 精确前置条件 | 处理 | 主要收益 | 竞赛风险/结论 |
|---|---|---|---|---|
| `.c/.i` 选择 | task definition 指定 | 原样使用指定文件 | 语义和 witness 一致 | 不再寻找 sibling `.c` |
| data model | task options 为 ILP32/LP64 | 传工具对应架构参数 | 避免 ABI 偏差 | 不支持就 UNKNOWN |
| assume wrapper | 以标准 `__VERIFIER_assume` 约定识别，或由 parser 证明某函数体精确为 `if(!cond) abort()` 并按符号绑定改写调用 | 映射到 verifier assume | 消除 abort 误分类 | 不得用普通函数名识别 benchmark；额外副作用/控制流即拒绝 |
| `reach_error` | 精确的 non-returning error wrapper | 映射到工具 error primitive | 识别 reachability | 必须保持源位置 |
| SV-COMP atomic function | 完整函数符合 atomic 命名约定且无不支持控制流 | 用工具 atomic begin/end 包裹 | 支持 atomic section | 嵌套、goto、异常形态拒绝 |
| manual pthread create/join | 签名匹配，create attr 为 null，target 是地址 | 映射到工具线程 API | 处理预处理/手写声明 | 非空 attr、复杂 ABI 拒绝 |
| pthread rwlock initializer | 工具已有等价 rwlock 语义 | 映射初始化器 | 解决前端缺定义 | 没有 rwlock 语义则拒绝 |
| C non-static inline | 单翻译单元，定义和调用可见且不会改变地址/链接语义 | `static inline` | 消除 unresolved external | 多文件/取地址/外部定义拒绝 |
| `malloc` runtime | 已确认该工具 allocation primitive 与任务 property 一致 | 映射到 modeled allocation | 消除 unknown external | memsafety 语义不同则拒绝 |
| `sleep` | 仅用于让出调度，无返回值/时间语义依赖 | 删除或映射 yield | 消除外部函数 | 正式转换需工具级语义证明 |
| finite function pointer | points-to 集被语法和类型证明为有限完整集合 | 生成直接 dispatch | 避免间接调用前端失败 | 不完整集合会漏行为，默认拒绝 |
| nondet type cast | 仅固定输入研究合同 | 从固定 seed int stream cast | 扩大研究可运行覆盖 | **不能证明 SV-COMP arbitrary nondet TRUE** |
| compiler `-O1` | 工具前端确需 materialize 特定 inline/builtin | 针对结构启用 | 减少 unresolved external | 全局启用会产生新 intrinsic/libc 调用 |
| `-ffp-contract=off` | 前端不支持 `llvm.fmuladd` 且禁 contraction 保持任务所需语义 | 针对浮点任务启用 | 避免 unsupported intrinsic | 浮点 property 需单独验证 |
| atomic runtime flags | 源码含 `_Atomic`/`stdatomic`/`__atomic_*` | 关闭已知不兼容 reduction | 避免错误剪枝 | 只能按语义特征选择 |

优先使用 Clang AST/LLVM IR 或工具已有 parser 做 detector。正则只能用于结构极小、
形态完全封闭且 `apply()` 会二次验证的规则。

## 5. 明确拒绝的转换

以下失败经验要写进每个工具 runner 的 unsupported 检查：

- 条件变量：GenMC 公共运行时没有已证明完整的 `signal/broadcast` 语义；把它们暴露
  为内部函数曾使一个 false task 变成 blocked/true。不得用 no-op 或近似替换。
- pthread TLS key/destructor：没有实现时不得伪造线程局部语义。
- `PTHREAD_MUTEX_ERRORCHECK` 等 mutex 属性：忽略属性会改变错误行为。
- arbitrary nondet：固定 seed 只能完整探索该固定伪输入下的并发调度，不能覆盖任意
  输入。正式竞赛若没有完整 nondet 建模，相关 TRUE 必须降为 UNKNOWN。
- helper-thread nondet：注入线程改变事件图，并在实验中触发 typedef 冲突和 GenMC
  thread-create internal failure。
- invalid/uninitialized memory：不能自动当成 reachability false。
- 全局 `-O1`：会引入工具不支持的 LLVM intrinsic 和 libc call。
- 强制 LP64：不能用于 ILP32 task。
- 根据路径选择 `.c`、patch 或参数：违反 task 输入和 anti-fingerprinting 边界。
- 直接调用 GenMC 内置 SC/TSO checker 代替 CAT/CAAT：这绕过待评估算法，不是
  CAT/CAAT runner 优化。不同候选必须真实调用各自 backend。

## 6. 每个工具/方法怎样构造脚本

### 6.1 GenMC

ToolInfo：

- runner 名建议 `genmc-svcomp.py`；
- `REQUIRED_PATHS` 包括 GenMC binary、Clang/LLVM runtime、兼容头和所需 shared libs；
- benchmark definition 只放全局稳定选项，不放 task 路径规则；
- 提取 `complete executions`、`blocked executions`、peak graph/event counters。

runner：

- 仅支持已声明且正确实现的 property；
- `.i/.c` 不互换；
- 对 `_Atomic`、pthread、allocation、nondet API 做能力分类；
- `Verification complete` 只有在 exit 0、无 unsupported/internal diagnostics 且输入
  语义完整时才能成为 TRUE；
- exit 42/`Verification unsuccessful` 还需映射到当前 property 的具体 false 类型；
- race diagnostic 只在 `no-data-race` task 返回 `false(no-data-race)`。

当前最大正式参赛 blocker 是 arbitrary nondeterministic inputs。seed 1995 adapter
可保留为研究 candidate，但不能把 seed 下的 TRUE 提交为无条件 TRUE。

### 6.2 GenMC+CAT

复用 GenMC runner 前端和转换注册表，只替换 backend 配置：

- 模型文件必须包含在归档和 `program_files()` 中；
- 模型通过归档相对路径加载；
- SC/TSO/PSO 选择来自参赛 candidate 的全局配置，而不是 task 名/category；
- 输出必须包含实际模型 digest、CAT frontend version、checker backend；
- 不允许失败后静默 fallback 到内置 checker 并仍标记为 CAT。

### 6.3 GenMC+CAAT

与 CAT 使用同一预处理和 property 逻辑，差异只在 evaluator：

- 输出 offline/online/incremental backend、fallback reason、oracle/check counters；
- fallback 到 full CAT evaluator 可以保持正确性，但必须统计并在版本说明中公开；
- 不允许 fallback 到内置模型 checker 后仍报告 CAAT；
- rollback/rebuild/internal inconsistency 必须 ERROR/UNKNOWN，不能从部分状态返回 TRUE。

如果 CAT 与 CAAT 作为不同竞赛 candidate，分别提供 `genmc-cat.py` 与
`genmc-caat.py`，但共享一个经过测试的 Python library，避免结果规则漂移。

### 6.4 TruSt

- 先验证归档是否真实支持 task data model；旧 artifact 的 LP64-only 研究结果不能
  代替 ILP32；
- `--rc11/--sc` 和 `--mo` 是 candidate 的全局模型配置；
- `No Error detected` 只能在 exit 0 且探索完整标记存在时为 TRUE；
- `Error detected` 必须结合 property 和具体 diagnostic 返回 false subproperty；
- 不同版本输出文本不同，parser 必须按归档版本固定精确模式。

### 6.5 Awamoche

- 继承 TruSt 公共 runner library，但 binary、版本、模型 flags 单独声明；
- 不把 TruSt 的输出模式无条件复用，先为 Awamoche 建 golden logs；
- 内部估算/内存模型检测器若关闭，必须是 candidate 的全局配置并写入审计行；
- unsupported external、frontend crash、thread runtime failure 均返回 UNKNOWN/ERROR。

### 6.6 Mixer

- runner 显式记录 `--mixer`、SC/RC11 和内部线程数；
- 如果 Mixer 支持 SC 和 RC11，可作为不同 candidate/configuration，但不能按 task
  特征切换内存模型；
- 解析真实 Mixer 完成标记，不以“有执行数”单独推断 TRUE；
- 内部并行受 `rlimits.cpu_cores` 限制。

### 6.7 Spore

- 与 Mixer 类似，但保持独立 binary/version/output parser；
- 不能因为两者来自同一代码家族就共享未验证的 TRUE/FALSE marker；
- 为 SC/RC11、safe/unsafe、timeout/OOM/crash 各保存至少一个 golden log。

### 6.8 Deagle

BenchExec 已有官方 `deagle.py`，新版本应基于它修改而不是另起不兼容模块：

- 使用 task property 和真实 data model；
- Deagle 归档中的 `deagle` wrapper 已负责 unwind strategy，ToolInfo 不应为具体
  benchmark 设置 unwind；
- TRUE 必须建立在完整 unwinding/证明条件上；unwinding assertion failure 是
  UNKNOWN，不是程序 bug；
- `FAILED` 后按 `nodatarace.assertion`、pointer/alloc、memory-leak、overflow 精确映射；
- 审查 `--allow-pointer-unsoundness`：任何可能漏行为的配置都不能支持正式 TRUE；
- 避免宽泛 substring，例如先匹配 `SUCCESSFUL` 可能误吃包含该串的否定文本，使用
  完整行或结构化输出。

### 6.9 CBMC

BenchExec 已有成熟官方 `cbmc.py`，主要经验是不要破坏其完备性判定：

- property mode 传 `--propertyfile`，架构传 `--32/--64`；
- XML mode 应解析 `cprover-status` 和 failure reason；
- `FAILURE` 中的 unwinding assertion -> UNKNOWN；
- `SUCCESS` 只有在启用并通过 unwinding assertions 或其他完整性条件时 -> TRUE；
- OOM、SAT solver OOM、usage error、invalid XML 分开分类；
- 不因 exit 0 单独返回 TRUE。

## 7. 结果判定的固定优先级

每个 ToolInfo 的 `determine_result()` 使用同一顺序：

1. BenchExec `was_timeout` -> `TIMEOUT`；
2. memory termination -> `OUT OF MEMORY`；
3. 其他外部 termination 或 signal -> `ERROR`；
4. runner 唯一且合法的 `SVCOMP_RESULT=`，同时要求 wrapper exit 0；
5. runner `SVCOMP_STATUS=` -> 具体 ERROR；
6. 无 marker、重复 marker、冲突 marker -> `ERROR`。

runner 内部判定顺序：

1. adapter/preprocess unsupported；
2. compile/frontend/internal runtime failure；
3. property-specific violation；
4. explicit complete success；
5. explicit incomplete/unknown；
6. 未识别输出 -> adapter error。

不要用 `if "true" in output`、最后一行包含 `FALSE`、exit code 0 等宽泛规则。
每个工具版本保存 golden logs，并测试 marker 冲突、截断输出和混合 stdout/stderr。

## 8. 统计与审计证据

BenchExec 本身记录 wall time、CPU time、memory、exit code、signal 和 termination
reason。runner 额外输出：

```text
SVCOMP_STAT adapter.transforms=2
SVCOMP_STAT tool.executions=1543
SVCOMP_STAT tool.blocked=18
SVCOMP_STAT tool.solver_calls=27
SVCOMP_AUDIT={"adapter_version":"...","data_model":"ILP32",...}
SVCOMP_RESULT=true
```

审计 JSON 至少包含：

- adapter 与 verifier 版本；
- property kind 和 data model；
- 输入后缀，不记录用于调参的 task 名；
- applied transform IDs；
- backend/model 和全局 flags；
- fallback/unsupported reason；
- 是否生成 witness 及其格式；
- 输出 parser schema version。

不要在日志中打印服务器地址、密码、用户目录或 Docker 配置。归档必须能从任意路径
运行，只写当前工作目录和 `/tmp`。

## 9. soundness/completeness 验证门槛

每个工具脚本按以下顺序验证：

### P0：模块和归档

1. `python3 -m py_compile` 两层脚本；
2. `python3 -m benchexec.test_tool_info <tool>`；
3. `smoketest.sh` 从不同工作目录运行；
4. `program_files()` 打包清单在空环境中可执行；
5. normal container mode 验证，不以 `--no-container` 作为通过条件。

### P0：转换规则

每条 transform 至少有：

- exact positive；
- 少一个前置条件的 negative；
- 多一个副作用/控制流的 negative；
- `.c` 和 `.i` 适用边界；
- ILP32/LP64 边界；
- safe 与 unsafe task；
- 转换前后共享事件/线程/同步语义对照；
- witness 源位置检查。

### P0：结果 parser

为每个版本固定 golden logs：

- TRUE、每种 FALSE subproperty、UNKNOWN；
- timeout、OOM、signal、assertion/abort/segfault；
- compile error、unsupported external、invalid output；
- 部分输出和超过输出限制后的截断日志；
- 同时出现 success/error marker 的冲突日志。

### P1：小规模差分

- 原始工具支持的 task，wrapper 前后 verdict 和探索数一致；
- 转换 task 用独立工具/旧实现作 oracle；
- true-on-expected-false 与 false-on-expected-true 分方向审计；
- TRUE 的要求比 FALSE 更严格：任何 completeness 缺口均降 UNKNOWN。

### P1：全量训练集

1. 先用短限时定位 compile/unsupported/ABORTED/segfault；
2. 这些非资源错误降到接近零后再提高时间限制；
3. 汇总每条 transform 的覆盖、正确/错误/unknown 和开销；
4. 重点人工检查 wrong，而不是用 expected verdict 修 parser；
5. 保存完整 BenchExec XML、log、table-generator HTML 和脚本版本。

## 10. 新建一个工具脚本的最小步骤

1. 复制两个模板，分别放入 BenchExec repo 和工具 archive。
2. 固定 verifier binary、version、archive-relative dependencies。
3. 声明支持的 property、data model、memory model 和完整性条件。
4. 收集该固定版本的 golden logs，再写 parser。
5. 从历史转换表中只选已满足前置条件的规则。
6. 为工具独有 unsupported constructs 建 fail-closed detector。
7. 输出统一的 verdict/stat/audit markers。
8. 运行 ToolInfo utility、container smoke、规则单测和 parser golden tests。
9. 在训练集做短限时 census，按错误类型修复。
10. wrong 归零或有明确 soundness 解释后，再做正式长限时性能实验。

最重要的验收条件是：脚本面对未知形态时返回 UNKNOWN/ERROR，而不是通过近似转换
制造 TRUE；面对已知 bug 时，必须输出当前 property 的精确 FALSE 类型并生成可验证
witness。
