# GDSL Score — Post-Ceiling: 从「DSL 设计分 T」到「游戏语料分 G」

> 状态：**设计标准（proposed）**，不是已验实现。文中「已建」标的是此前真实跑通的 oracle / golden / playtest 机器；「新建」标的是本标准的语料、聚合器、门禁与 cron 契约，尚待实现。/ 本文回答的问题是：TSCORE 的 C/V/E/Q/D 已经**真饱和**（T=100，五维经真实测量确认真饱和，非测量假象），那么「score 一个优化 GDSL 的 agent」该怎么打，才不重新踩进 Goodhart。
>
> — 锚点：GDSL 的 DSL 设计本来就在 `doc_ai/TSCORE.md`；本文是它的接续（outcome 层）。

---

## 0. 一句话：怎么打分 + 为什么（essential）

**怎么打分。** 不再给「DSL 的设计」打分（那已饱和），改给「DSL **能正确产出的游戏**」打分：

```
G = Σ(corpus 里「agent 生成的 GDSL 解析过 + 编译过 + 真机 play 对 golden」的任务) / Σ(corpus 任务)
```

每个 corpus 任务 = 一个（难度分层的）目标游戏/场景，配一份**人的 golden 行为规约**（引擎必须真的做出什么），外加一份**参考 GDSL**（证明这个任务「可表达」——没有可编译的参考解 = 不可能任务，不得计入，否则是坏指标）。

**为什么这分不会被 Goodhart。** 四个机制，各堵一类反模式：

1. **证据是真实引擎 playtest** —— 退出码 + 状态断言进记录，不是 agent 的 prose 自述、不是它可删的哨兵 token。→ 堵「验证伪造 / reward hacking」（Sakana DGM 记的「伪造单元测试日志」「删掉自己幻觉检测器标记」就是这类）。
2. **golden 来自人的规约，绝不来自配方本身** —— 不许对着「配方自己写的 effect」断言。→ 堵「循环论证」（agent 写错但自洽的 `-=1` 会被它自己写的 effect 放过去）。
3. **每个新构造必须「买到」一个原本不可表达的游戏** —— 反膨胀耦合（Q'）。加一条语法产生式/ontology 符号，只有当它能让你表达 ≥1 个此前 DSL 表达不了的 corpus 游戏时才算分；只覆盖单个手挑样本 = 净负。→ 堵「token stuffing / 语法膨胀」。
4. **headroom 只能来自「真正更难的 tier」** —— 且每个新 tier 必须先证明「有参考解 + 真机 play 对」才能拉高门槛。→ 堵「饱和后靠换口径/堆 mutant 制造空间」的 Goodhart。

一句话总结四者的关系：**这是一套「真值外部化 + 目标不可代理 + 收益与真实能力耦合」的分数**，所以它是一个可刷到一个非常高的点、但没法靠刷假分虚涨的分。

---

## 1. 为什么 TSCORE 到天花板后就不能再当竞争分

TSCORE §4 的结论（2026-09-03 实测）已经说死：C=1.0（5 个 harder 任务 LLM bench 5/5 首轮即中，非测量假象）、V=1.0（validator 16/16）、E=1.0（**语义口径** 8/8，真机 playtest）、Q=1.0、D 稳定 → T=100。**这是真天花板，不是测量太简单。**

关键推论：**当所有维度都真饱和，任何「诚实改进」都已顶格。** 这时要创造竞争，只有两条路：

- (1) **重新武装同一套测量**——堆 mutant、加 harder 任务、改权重/目标——**如果新测量不是真的更难，这就是 Goodhart**（TSCORE 自己已经判定「禁止」）。
- (2) **换一个测量目标**——把「被测量的东西」换成一个有真实、无界、不可代理 headroom 的目标。这才是诚实的选择。

本文走 (2)。被测量的东西从「DSL 的设计质量」（已饱和的 proxy）换成「DSL 产出的游戏质量」（无界）。

---

## 2. 新的测量目标：游戏语料（golden game corpus）

DSL 的存在是为了产出游戏。所以去量它产出的东西。**语料**是计分对象，每个条目：

| 字段 | 内容 | 作用 |
|---|---|---|
| `game` | 目标游戏/场景描述（人类可读） | 定义「要表达什么」 |
| `ref.gdsl` | **参考 GDSL**（必须能 `gdslc logic` 通过） | **证明可表达**——没有可编译参考解 = 不可能任务，不得计入 |
| `golden` | **人的行为规约 / 期望值**：引擎里 `{setup: 字段=初值, call: 方法, expect: 字段=期望值}` | 计分真值；**独立于配方**，绝不从 ref.gdsl 里反推 |

**难度分层**（这是「无界 headroom」的来源——tier 可以一直加）：

- **T1 基础**：1 个 type、1–2 条 rule、简单 effect（assign/add/sub）。
- **T2 交叉**：跨 type 的 `target:`、多条 rule、`signals:`/`emit`。
- **T3 复合**：≥3 条 rule、≥2 个 type、emit+gating 组合、生命周期（增删实体到终点态）。
- **T4+ 更难**：后续新增，每个必须在加入前验证「有参考解 + 真机 play 对」。

---

## 3. 怎么打分（机械定义 + 子维）

### 3.1 子维（不是给一个裸比例，要给多维，才不会退化成单代理）

- **C' correctness（语义正确）** —— 真机 play 对 golden 的比例。**这是主 outcome，权重最大。** 用 `semantic_bench.py` / `loop_bench.py` / `golden.py`（FR-010/011）这一套已建机器：`ALLPASS/(ALLPASS+FAIL)`。
- **E' coverage（可表达）** —— corpus 里「原本表达不了、现在能表达」的比例。**这是 feature 工作（加语法/ontology）真正得分的地方。**
- **Q' restraint（克制 / 反膨胀）** —— `新增可表达游戏数 / 新增构造数`（构造 = grammar productions + ontology 符号）。比率越高越克制；一个构造只覆盖一个手挑样本 = 净负。→ 与 TSCORE 的 economy 同一思想，但这里耦合到**真实游戏**，不是耦合到覆盖率计数。
- **D' determinism** —— codegen golden 逐字节稳定，归入 law floor，不计权（与 TSCORE 相同）。

### 3.2 权重提案

```
G = 55·C' + 25·E' + 20·Q'      (D' 归入 law; 任一 NA 维归一, 其余权重重算)
```

> C' 是实际结果，最重；E' 奖励扩展能力；Q' 惩罚膨胀。权重只是建议——**改权重必须写进本文档并说明理由 + 该维是否饱和 + 是否被语料支持**（沿用 TSCORE 的「改标准纪律」），不许静默改。

### 3.3 Law floor（二值门，违约 → G=0 / DISQUALIFIED）

- **L1 游戏 oracle 绿**：至少 T1 层真机 play 对 golden（保证「加一条语法但不真 play」拿不到分）。
- **L2 determinism golden 稳定**。
- **L3 可参考性**：凡计入 G 的 corpus 任务，必须已验「存在参考解且编译通过」——**不可能任务不得计入**（它是坏指标）。
- **L4 诚实**：没有为凑绿而弱化的测试（diff 测试文件）、没有虚构符号（每个 `@extends/class/signal/effect` 都能 grep 回 `extension_api.json`/ontology）、没有过度声明的文档（每个 "done" 有测试背书）。

---

## 4. 为什么这分不会被 Goodhart（why，展开）

### 4.1 堵「验证伪造」（DGM 的 reward hacking）
证据必须是**真实引擎 playtest 的退出码 + 状态断言**，进记录；evaluator 是**fresh 进程、零实现上下文**；golden 是**密封**的。agent 伪造不了运行日志（它没跑真引擎），也删不掉信号（信号是真实场景/节点的行为属性，不是插进去的哨兵 token）。TSCORE 的记录里已经见过 `hard_composite.gdsl` 走 240s 超时的 harness 慢问题——**这必须先解决**（换等价断言 / 分片跑），否则 T3 无法成为可靠 tier（详见 §6）。

### 4.2 堵「循环论证」
golden 从人的规约来，**永不从配方反推**。agent 不能靠「断言自己写的 effect」让「写反但自洽」的配方过——期望值是人定的，配方只是被测对象。

### 4.3 堵「语法膨胀 / token stuffing」
Q' 耦合：**新增一条语法产生式 / 一个 ontology 符号，只有当它让自己能表达 ≥1 个此前表达不了的 corpus 游戏时才得分。** 只服务单个样本的构造 = 净负。这直接对应 generic skill 里 anti-monomorphism（别把同一个 class 贴满所有按钮）在 DSL 上的等价物——在 DSL 上是「别加一个只服务一个样本的语法」。

### 4.4 堵「饱和后制造 headroom」
headroom 只能来自「一个**真的更难**的 tier」，并先证明它可参考、可真机 play 对。若新 tier 写不出参考解 = 不可能任务 = 排除，不算「分变低 = 测量变难」。

### 4.5 真实的 V↔Q 张力（这是「品味分」而非「一元代理」的地方）
C'（正确性）与 Q'（克制）**真的会冲突**：堵一个正确性缺口往往要加语法（如 `signals:` 声明）→ 压 Q'。这个多维张力正是品味分的意义——agent 必须在「多表达」与「别膨胀」之间平衡，而不是把一个 proxy 刷到顶。**Sakana DGM 记录的「加 patch validation / 更好的 file view」这类 self-improvement，在本分里会体现为「E'↑ 但 Q'↓」的净权衡，由 agent 自己权衡。**

### 4.6 诚实饱和
G 到头顶时，**照实报「饱和，无区分度」**，绝不假装「还有提升空间」来显得严。诚实的下一步只能是 harder tier（新测量 → baseline reset → feature 工作），而每个新 tier 必须先证明它真的更难（更难写对 + 参考解存在）。

---

## 5. 和 TSCORE 的关系（别打破旧的）

- **TSCORE 退为「设计质量地板」（law/metric 门槛）**：DSL 是否保持克制、可验证、确定性、LLM 友好——不许回退。它是 gate。
- **G 是「优化 agent 的 outcome 分」**：那才是优化 agent 竞争的度量。
- 两层分级：**structure=gating, outcome=metric**（正是你 anti-Goodhart skill 里的 separation）。TSCORE 的「静态结构分爬到顶 ≠ 品味分」教训在这里复用：TSCORE 是「没做坏」，G 是「做出了对的游戏」。
- **任何计分规则变更（新 tier、改权重、改 golden）→ baseline reset**，旧分数不可比；否则「beat the max」的门槛会变成不可满足（TSCORE 已踩过）。

---

## 6. 已建 vs 新建（诚实分层，不把 mock 当已验）

**已建（真实跑通，可作为本分的积木）**：`semantic_bench.py` / `loop_bench.py` / `golden.py` / `playtest.py`（FR-005/009/010/011）——真机 playtest + golden + 突变审计；`gdsl/toolchain/tscore.py` 的聚合器、`.wolf/tscore_leaderboard.jsonl` 的 leaderboard、cron monitor `--fingerprint` 空转断路器。

**新建（尚待实现）**：
1. **corpus**：分层 golden 游戏规约 + 参考 GDSL + 独立 golden 期望值。这是本分的主体，也是要投入最多的地方。
2. **聚合器**：把每游戏 ALLPASS 转成 G（含 E' 的「原本不可表达」基线、Q' 的「构造→可表达游戏」记账）。
3. **门禁**（L1–L4）+ **cron 契约**：沿用 TSCORE 的契约（monitor 指纹、baseline reset、不 push、surgical staging、self-review checklist）。

**待验证 / 未知**：`hard_composite.gdsl` 语义 playtest 走 240s 超时——这是 harness/泛型脚本慢、不是语义 bug（TSCORE §4「唯一未知」）。**T3 要成为可靠 tier 必须先解决它**（等价断言 / 分片 / 加超时策略），否则 T3 无法进基线。

---

## 7. 交付纪律（诚实）

- 每个 G 数字都来自**真机 playtest**（永不把「编译过」当「play 对」）。
- 分开报：`解析过` / `编译过` / `真机 play 对` / `有参考解`——避免一个饱和的 T1 掩盖真正的短板。
- 每层标状态：**verified / unknown / not-measured**。例如 T1 已验，T3 因 harness 慢标 unknown，semantic-E 只覆盖已测例。

---

## 8. 一行 recipe（怎么用）

```
1. corpus: 分层 golden 游戏规约 + 参考解 + 独立期望值
2. 每个 agent attempt: 拿「最难/最便宜且有真实 headroom 的 tier」当目标,
   先写失败 repro (red) -> 最小改动 (green) -> 真机 playtest -> 得 G_after
3. 只有 laws_pass 且 G 严格提升 (分层) 才留, 否则回滚
4. G 到头顶 -> 报饱和; 唯一诚实杠杆 = 加一个「已验可参考可真机 play 对」的 harder tier
   (新测量 -> baseline reset -> feature 工作)
5. cron: monitor 指纹 + baseline reset + 不 push (沿用 TSCORE 契约)
```

**反 Goodhart 检查清单（提交前，全部 yes 才 commit）**：
- [ ] G 的提升来自「真机 play 对的新游戏」或「新表达的原不可表达游戏」，**不是**阈值放宽 / 语料删减 / 参考解删了。
- [ ] 每个新构造都 grep 回真实语法/ontology 且在 corpus 里解锁了 ≥1 个此前表达不了的游戏（Q'）。
- [ ] 没有为凑绿弱化的测试（diff 测试文件）。
- [ ] 没有伪造符号（`@extends/class/signal/effect` 都能 grep 回 `extension_api.json`/ontology）。
- [ ] golden 来自人/规约，不是从配方反推。
- [ ] 只 stage 自己的文件；probe/temp 清理；不 push。
