# 让 agent 持续 / 自治 / 连续跑 8 小时 —— 规则与证据

> 一句话：自治 agent 能 24 小时干活，靠的不是「不会失败」，而是「契约 + 状态 + 可恢复」三件套 + 每阶段一条 Gate 收口。铁证：144 个模块文档化，1 小时 22 分跑完、148 次派发、4 次瞬时失败全恢复、零进度丢失（`JOURNEY.md:371`）。8 小时专心攻坚那次的骨架是「控制实验 → 抓栈 → 定 fix → 落地 → 回归」，每阶段一条 Gate（`doc_ai/PLAN_GDSL_8H.md`）。

## 是什么 / 不是什么

- 是「先立契约，再放手派，事后核账」；不是「盯住每个 agent 不许出错」。
- 是「状态机 + 幂等认领」；不是「定时器轮询」。
- 是「可恢复是一等公民」；不是「假设每次调用都成功」。
- 是「Gate 收口自动探究」；不是「让 agent 无限自由发挥」。

## 三件套（缺一不可）

1. **契约放共享文件，prompt 放最小指令。** `DOC_SPEC.md` + `INDEX.md` 承载全部纪律，每个子 agent 的 prompt 只有 ~15 行指向契约。这是 144 次派发便宜、可扩展的原因（`JOURNEY.md:375`）。纪律别塞进每次 prompt，放进「可检索的一处」，prompt 只给入口。

2. **状态机 + 幂等认领，不是定时器。** `[ ]/[~]/[x]` 三态 + 「先认领再动手」，7–14 个并发不撞车、崩溃后能续（`JOURNEY.md:373`）。14 批并行无一次重复写同一模块。

3. **可恢复(resume)是一等公民。** 148 次派发 4 次失败（2.7%）——1 次 prompt schema、3 次 connection reset——全部 `task_id` resume 恢复（`JOURNEY.md:374`）。默认每次工具调用都可能失败，把恢复路径设计进去，不是祈祷成功。

## 质量纪律要写进「法律」，才能穿透自治 agent

「写前 grep 确认、不编造符号」是硬规则，于是子 agent 反复纠正主 agent 的错误提示——nanosvg→ThorVG、MultiplayerReplicator→SceneReplicationInterface、InputFilter 不存在（`JOURNEY.md:376`，`SOP_TDD_AI_TESTING.md:60-78`）。纪律停在口号就没人执行；写进契约（law）才会被执行。

## 追踪器要对账地面真值

自治 agent 的追踪器会漂移。144 模块那次主 agent 写「145 / modules 58」，实际是「144 / 57」，靠 INDEX vs 文件系统对账抓出来（`JOURNEY.md:377`）。**不能只信自己的 tracker**，必须有独立的、不依赖 agent 自己的对账来源。

## 并行 >> 串行 cron

「每 5 分钟 1 个」串行要约 12 小时；14 批并行 64 分钟，约 11 倍（`JOURNEY.md:378`）。7×24 的价值在吞吐，不在「定时」。**能并行的不要排队**。

## 人干「立契约 + 事后核账」，不盯每个 agent

杠杆在前（追踪器 + 契约 + 任务）和后（对账计数、抽查质量），中间 fire-and-forget 等通知（`JOURNEY.md:379`）。

## anti-drift：持久化目标 + 每轮重注入

「AI agents drift across long sessions — 用持久化目标 + 每轮重述完整目标与当前切片的 anti-drift prompt」（`SOP_TDD_AI_TESTING.md:385-399`）。自治 agent 的慢，多半不是写得慢，是慢慢漂走。目标要每轮重注入，agent 不得：
- 过一个测试就宣称完成（还有剩余 seam 时）；
- 跳过红阶段省时间；
- 「改进」超出当前测试所需的最小实现（不投机）；
- 把「测试通过」当免 review 的通行证。

## Gate 收口：自动探究也要有护栏

8 小时那次的骨架（`doc_ai/PLAN_GDSL_8H.md`）证明：自动探究若没有 Gate，会无限查下去没产出。它把任务切成「控制实验 → 抓真栈 → 定 fix → 落地 → （尾巴）验收」，每阶段一条明确 Gate，外加两条硬禁令：
- **禁止：还没做对照实验就定 fix**（`PLAN_GDSL_8H.md:46`）——先分清「上游卸载顺序 bug」还是「本项目生成代码特有」，再动手。
- **不拿栈不写 fix**（`PLAN_GDSL_8H.md:52`）——crash 根因必须有符号化调用栈坐实，禁止仅靠源码路径推理下结论。（对齐 `.wolf/cerebrum.md` Do-Not-Repeat：`[2026-08-31]` 曾凭推理改 `p_is_static` 且声称 teardown 根因，两处皆错。）

教训：**阶段化 + 每阶段可证伪 Gate + 明确禁止越级**，是「自动探究 8 小时还不跑偏」的护栏。

## 验收口径（诚实标注，不粉饰）

- 完工 = 每条验收标准有真实命令输出为证，从 live tracker + 实跑结果回答，不从记忆/声称。
- 「编译通过」≠「运行时正确」；law 可证伪（一例即违约）、metric 只记录趋势、morality 靠 review（`SOP_TDD_AI_TESTING.md` §0.5 三层）。
- 报告严格分开「已度量 / 未度量 / 未知」，不把未验证的说成已验证，也不把非目标算成缺口。

## 数据出处

- `JOURNEY.md:369-379`（7×24 自治 agent 的启发，含 6 条经验）
- `doc_ai/PLAN_GDSL_8H.md`（8 小时攻坚的 Gate 骨架 + 两条硬禁令）
- `doc_ai/SOP_TDD_AI_TESTING.md`（§0.5 三层、§4.5 anti-drift、§4.2 红前绿后）
- `.wolf/cerebrum.md`（Do-Not-Repeat：`p_is_static`、声明缺口前先读源码）
