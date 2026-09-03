# gdsl TASTE SCORE — 隔夜多 agent 竞争标准

> 一句话：给 GDSL 一个机器可算的「品味分」T=[0,100]，多 agent 各自改 DSL/语法/ontology/文档，跑 `gdsl/toolchain/tscore.py` 刷分，night 轮跑看 leaderboard 谁在最上。

**结论：能做，且这个分数今天就能跑（`python3 gdsl/toolchain/tscore.py`，秒级，不调 LLM）。当前基线 T=97.8/100。但必须诚实指出一个设计结果：今天这分**几乎全维度饱和**——C/V/E 都是 1.0，只有 economy(Q)=0.889 有真实区分度，竞争区间其实是 [80,100]。这不是 bug，是「测量集太简单 + mutation 没测」的诚实体现。要让隔夜竞争真正有意义，第一步得加更难的任务集 + 建 mutation harness（FR-005）。**

（锚点均为仓库内真实产物，可用 `grep`/`ls` 回验；基准分来自 2026-09-01 实跑 `tscore.py`。）

## 是什么 / 不是什么

- 是「把 GDSL 的设计品味量成一个可刷的分」；不是「把代码好坏量成分」——分是给**设计决策**的（语法加不减、ontology 扩不扩、文档诚实不诚实），不是给单次代码 diff 打分。
- 是「跨尝试可比、可复现、无人值守可跑」的度量；不是「一次性的评审意见」。
- 是「law(门) / metric(分) / morality(人审) 三层」的 metric 那层；不是把软偏好（比如 ontology 命名）硬塞成分数。

（以上是对比句的全部，全文 ≤3 处。）

---

## 1. 五个维度 + 三个门槛

**门槛（law，二值，违约 → T=0 / 不合格）**，这是「入场券」，不是竞争分数点：

| # | 门槛 | 判据 | 当前 |
|---|---|---|---|
| L1 | doctest 全绿 | `test_gdsl.exe` 全部 test case + assertion 通过（内含 determinism golden 逐字节 + FR-009 报错定位测试） | ✅ 97/403 全绿 |
| L2 | 结构覆盖不回落 | `playtest_cases/*.gdsl` 过 `gdslc logic` ≥ COVERAGE_FLOOR(6) | ✅ 8/8 |
| L3 | LLM 能写出 | convergence `valid_rate` ≥ 0.75（有 bench 数据才判） | ✅ 1.00 |

**五个维度（metric，[0,1] 归一，加权求和）**：

| 维 | 含义 | 测法 | 归一 | 目标 | 当前 | 饱和? |
|---|---|---|---|---|---|---|
| **C** convergence | LLM 多快能写出合法 GDSL | `gdsl/bench_results.json`（FR-006/007/008）| `0.50*first_try + 0.30*valid + 0.20*iter_ok`（iter_ok=1-(mean_iters-1)/4）| iter 1–3 | 1.000 | **饱和**（首轮全中） |
| **V** verifiability | 可验证性 | doctest 全绿 + FR-009 报错定位测试 + **FR-005 mutation 抓突变率**（tscore 跑 15 个 mutant 看校验器真实拒收率） | `0.50*all_pass + 0.20*err_quality + 0.30*mutation_score`（NA 维归一） | 错误可定位 + 坏输入必被拒 | 0.94 | **有区分度**（mutation 12/15） |
| **E** expressiveness | 真的能说这些玩法（且真的玩对？） | **语义口径**：`semantic_bench.py` 跑真机引擎 playtest，`playtest_cases/*.gdsl` 中 ALLPASS（真的按 golden 玩对）的比例；无 `semantic_results.json` 时回退**结构口径**（parse+typecheck+codegen 通过率，FR-010） | `ALLPASS/(ALLPASS+FAIL)` | 覆盖难样本 + 真玩对 | 1.000 | 结构饱和；(待语义) |
| **Q** economy | 品味/克制 | 每构造覆盖的真实任务数 `covered/(grammar_prods+ontology_syms)` | `ratio/ECONOMY_TARGET` clamp≤1 | ratio→0.9 | 0.889 | **有区分度** |
| **D** determinism | 免漂移 | codegen golden 逐字节 + hash 黄金值稳定（在 doctest 内）| 归入 L1 门槛，不计权 | 逐字节稳定 | 稳定 | — |

### 两个「真区分度」在哪儿（诚实标注）

现在有**两个**能动的分：**V=0.925**（mutation 9/12，3 个真实缺口）和 **Q 已到 1.0**（满分，无余地）。C/E 饱和**不是**「设计满分的好消息」，是「测量集太简单」——所有样本首轮即中、8/8 全过，所以 C/E 测不出差距。三条真实撬棍：

1. **加更难的任务集**（最难、也最有品位价值）：现在的 `bench_results.json` 8 个任务 + `loop_composite.json` 2 个，全是容易的。一个能区分高手的 taste score，必须含「大多数 LLM/agent 一次写不对、需要 2–3 轮反馈」的样本（对齐 SOP §1.1 iterations-to-green 1–3）。加进去，C/E 才真正动。
2. **V 的 mutation 缺口 = 当下的主杠杆**：tscore 跑 12 个 mutant，校验器抓到 9/12，**3 个真缺口**——`string_num_lit`（string 字段被赋数字字面量）、`string_from_num`（string 字段被赋 int 字面量）、`typo_signal`（`emit(hitt)` 拼错信号名无存在性校验）。它们都会编译成错/失效代码，是真实可验证性缺口（LLM_UNFRIENDLY #4 的「待钉死」政策 + emit 信号存在性）。**堵住它们 = V 提升 = T 提升**。（注：堵 string 字面量便宜；堵信号存在性属 v2——需要信号声明或查 Godot 基类信号集，更贵。）
3. **V↔Q 权衡**：若要堵信号存在性，往往得给 DSL 加 `signals:` 声明（加 grammar）→ 可能压低 Q。这是 score 里真实出现的品味张力：可验证性与克制互斥，agent 要平衡。

---

## 2. 加权公式 + 竞争区间

```
T = 100 × ( 0.30·C + 0.25·V + 0.25·E + 0.20·Q ) / Σ(现维权重)
     └ C/V/E 若同步饱和 -> 这部分退化为常量 0.80, 唯一可动的是 0.20·Q
门槛违约 (L1/L2/L3 任一 False) -> T := 0
```

- **权重按「现维」逐一归一**：某维 NA（如 mutation 未测→V 的部分权、无 bench→C）就重新归一剩余维度。不硬算、不填 0。
- **动态区间（现状）**：C/V/E=1.0 是常量 0.80，Q=0.20 在动 → 今天竞争区间 `[80,100]`。这是「只刷 economy 才能涨分」的现状，正是测量集太简单导致的。

### 反古德哈特（为什么这分不会被刷作弊，对齐 SOP §0.5）

- **law / metric / morality 不混层**。软偏好（ontology 命名、文档措辞、是否"未来可能用到"的抽象）是 morality，靠人 review，**不进分数**。把软偏好塞成分数 = 假精度，CI 一慢机器就红，人接着禁用。
- **饱和维度不装忙**。C/E 到 1.0 就照实报「饱和，无区分度」，绝不假装「还有提升空间」来显得严。绿色粉饰是你明确反对的失效模式（你自己的 Do-Not-Repeat）。
- **门槛不换算成分**。L1/L2/L3 是「合格/不合格」，违约即 0。绝不为多拿 Q 的分而放松 L1（比如跳过报错定位测试换 economy 涨分）。
- **不改门槛尺度来讨好 leaderboard**：ECONOMY_TARGET、任务集难度、mutation 阈值都是参数，任何人想改必须写进这个标准并说明理由，不是静默调。

---

## 3. 隔夜竞争循环（怎么跑）

1. **每个 agent = 一次「attempt」**，拿到本标准 + 一个明确目标（如「把 Q 从 0.889 提到 ≥0.95 且不破 L1」或「加 3 个更难任务让 C 出现区分度」）。
2. **改** DSL/语法/ontology/测试集/文档里它 owns 的那部分（沿用 AGENTS.md 文件所有权分区，别碰别的 agent 的路径，别 `git add -A`）。
3. **跑分**：`python3 gdsl/toolchain/tscore.py`（秒级、无 LLM、无引擎）。它把 `T + 各子分` 追加进 `.wolf/tscore_leaderboard.jsonl`，并写 `gdsl/tscore_report.json`。
4. **筛选**：只有 `laws_pass=True` 且分数**提升**的 attempt 才留；分数不升 → 回滚（attempt 不保留）。违约 → 直接不合格。
5. **排名**：leaderboard（jsonl）按得分排序，隔夜轮跑看谁在最上。各 agent 的产出不得互相覆盖已有文件（沿用所有权分区）。

### 让它无人值守（cron）

用 Hermes cron 起一个「调度」job：每段（如 2–4 小时）唤醒一个 agent attempt；每个 attempt 内部先 `git status` 对账（追踪器漂移风险，GDSL_LONG_RUN），跑 `tscore.py` 记录，是否保留依 T 是否升。调度 job 本身不写代码，只派活 + 记榜。**注意**：标准文档本身（本文件）也是被迭代对象——agent 可以提「加维度 / 改权重」的 PR，但必须走本文档的「改标准」纪律（写明理由 + 该维度是否饱和 + 是否被测量集支持），不能静默改。

**空转断路器（必须启用）**：给 cron job 设 `monitor = <gate脚本>`（无 LLM、每秒级、确定性指纹），指纹输出相同 → 本轮 agent 整个跳过。这样当分数饱和（score=100 且 gaps=none）时，cron 不再烧完整 agent run 只为得到「没东西可改」——空转降为零成本。**再武装测量（扩 mutant 集 / 加 harder 任务 / 重置基线）时指纹变化 → agent 自动唤醒。** 指纹必须无时间戳、无随机序（否则每 tick 都像变了）。

---

## 4. 当前基线（2026-09-03 `tscore.py`，E 已换语义口径）

```
T = 100.0/100   laws_pass=True     (L1,L2,L3 全过)
C = 1.000   V = 1.000   E = 1.000   Q = 1.000
E_mode = semantic   semantic_e = 8/8 (真机 playtest，生成的游戏真的 play 对)
mutation FR-005: 16/16=1.000   gaps=none
playtest(结构): 9/9 ；economy prods=5 constructs=9 covered=9 ratio=1.0
doctest: 97/403 全绿
```

**五维全真饱和（2026-09-03 实测）**：C=1.0（5 harder 任务 LLM bench 5/5 首轮即中，非测量假象）；V=1.0（validator 16/16）；E=1.0（**语义口径** 8/8，真机 playtest 确认游戏真的玩对）；Q=1.0；D 稳定。

**唯一未知**：`hard_composite.gdsl` 语义 playtest 时报引擎 240s 超时（该例无对应 golden 断言，playgen 泛型脚本对组合规则偏慢）——判定为 harness/脚本慢，**不是可证实的语义 bug**，归为「无法验证(unknown)」，不当作 headroom。

**反古德哈特最终结论**：全五维经真实测量确认真饱和。**继续堆 mutant / harder 任务 / 换口径制造 headroom 都是 Goodhart，禁止。** 这不是测量太简单——是 DSL 本质足够好。若还想让 cron 有大活，唯一方向是**给 DSL 加新功能以表达更难的游戏**（feature 工作，非 score 优化）。

**诚实前沿（下一步）→ [`doc_ai/GDSL_SCORE_POST_CEILING.md`](GDSL_SCORE_POST_CEILING.md)**：本分（T）是设计质量的 **gate / 地板**，已到天花板；真正有真实 headroom 的竞争分是「**DSL 能正确产出的游戏**」——见接续标准里的**游戏语料分 G**（真机 playtest 对 golden + 新构造必须解锁一个原本不可表达的游戏 + headroom 只能来自「已验可参考可真机 play 对」的 harder tier）。两层分级：**structure=T(gating), outcome=G(metric)**。T 在此后只作「不滑坡」的门槛，不再当优化 agent 的唯一记分。

---

## 5. 待确认 / 未测（诚实标注）

- **mutation score（FR-005）已测，mutant 集扩到 15 后有 3 个真缺口**：tscore 跑 15 个 mutant，校验器抓 12/15（0.80），漏 `dup_rule_name`（同类型重复 rule 名 → codegen 注册重复方法）/ `int_overflow`（int64 溢出字面量 → 生成错误值）/ `guard_type_mix`（`spd == 5` int→float 隐式比较，违反「无隐式转换」政策）。**堵住它们是当前最便宜、最诚实的 V 提升路径。待动作（非待确认）。**
- **语义正确性（不只是能解析）**：`playtest_cases` 只测了「过 gdslc」（结构），`loop_bench.py`/`golden.py` 的「编译 + 真机 playtest 双阶段」是更强的语义口径（FR-011），但跑起要引擎、分钟级。当前 E 是结构口径。是否把 E 换成语义口径（E_semantic）**待确认**——语义更难、也更像"真的能玩"。
- **难度校准**：hard 任务集的收敛率无数据（现有样本全 100%）。hard 集建成后的 first-try/iterations 分布**待确认**——它是 C/E 能否重启区分度的前提。

---

## 6. 数据出处

- 本标准：`doc_ai/TSCORE.md`（新）
- scorer：`gdsl/toolchain/tscore.py`（新，吃下面所有产物）
- leaderboard：`.wolf/tscore_leaderboard.jsonl`（新）；单次报告：`gdsl/tscore_report.json`
- LLM 收敛数据：`gdsl/bench_results.json`（FR-006/007/008，16 条）
- 结构覆盖集：`gdsl/playtest_cases/*.gdsl`（8 份）；难样本线索：`gdsl/loop_composite.json` / `gdsl/loop_results.json`（FR-011 语义闭环）
- doctest 套件：`gdsl/test_*.cpp`（97 用例 / 403 断言）；FR-009 两条错误定位测试在 `test_parser.cpp:251`、`test_typecheck.cpp:231`
- ontology（enum，非字符串）：`gdsl/effect.cpp`（`EffectKind`：assign/add/sub/emit）；语法产生式：`gdsl/parser.cpp`（`parse_*` 6 条 + 入口 parse_program）
- 规则哲学：`doc_ai/SOP_TDD_AI_TESTING.md` §0.5（law/metric/morality、反古德哈特、不混层）；`doc_ai/GDSL_LONG_RUN.md`（并行、对账、Gate）；`doc_ai/LLM_FRIENDLY_ENV.md`（这分是它的 metric 层落地）
