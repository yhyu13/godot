# JOURNEY.md — godot 引擎 fork 的引导阶段记录

> 本文件记录「人怎么决策、纠正、砍方向」与「AI 怎么执行、证伪、诚实报告」的时间线。
> 列名：`ME = 用户`，`YOU = AI`。日期格式 `YYYY-MM-DD`。
> 当前阶段：**LLM 友好 DSL 开发中**——设计文档（两轮 critic 收敛零 blocking）+ 实现计划 + `gdsl/` parser 内核（type / state 两片 red-green，9 用例 23 断言全绿）。上一阶段：144 子系统文档化 commit `83bd709`。

## 风险与待办（roll-up，非替换——各时代的行内风险/待办仍在原表里）

- **[Era 1·起步] 浅克隆 + fork 远端**：`git rev-parse --is-shallow-repository` = `true`，仅见一个 commit `f6ab5db`；remote 是 `yhyu13/godot`。**不能假设能 push 上游 `godotengine/godot`**，PR 走 fork 流程。
- **[Era 4·OpenWolf] web-app 模板与 C++ 引擎错配**：`.kilo/command/{reframe,security-audit}.md`、`.claude/commands/*`、`.wolf/OPENWOLF.md` 的 `designqc`/`reframe`/`npm audit` 都是 React/Tailwind/web 套路。agent 照做会落空（本仓库无 `package.json`/dev server）。待确认：裁剪成 Godot 适用，还是接受不适用。
- **[Era 4·OpenWolf] 记忆库全空**：`.wolf/{cerebrum,memory,STATUS}.md`、`buglog.json`、hippocampus/token-ledger 均 15:54:07 初始化为零。TODO：真实开发开始后按 OPENWOLF.md 协议回填，否则 OpenWolf 形同虚设。
- **[Era 4·OpenWolf] anatomy.md 对 C++ 文件价值低**：`file_count=513, hits=0`，多数描述只是版权首行（如把 `CLAUDE.md` 标成「OpenWolf」）。RISK：agent 依赖 anatomy 描述会读到无意义文本。
- **[Era 2-3·AGENTS] 两份指令文件会漂移**：`AGENTS.md`（紧凑）与 `CLAUDE.md`（更详）内容重叠（构建/测试/架构/贡献约定）。TODO：后续改动若只更新一份，另一份会过期。
- **[Era 8·push] `.wolf/` 运行时状态文件被提交进 git**：`hippocampus.json`/`token-ledger.json`/`_scan-state.json` 等含机器绝对路径 `D:\GitRepo-My\godot`，会随每次会话变化产生提交噪声。待确认：是否给 `.gitignore` 加规则排除。

---

## Era 1 — 起步：Godot 引擎 fork（浅克隆）落地

| ME | YOU |
|---|---|
| 建立了 `yhyu13/godot` 的 fork，checkout 到 `4.7` 分支（浅克隆，只拉了一个 commit）。 | 确认仓库身份：Godot Engine 源码，`version.py` = 4.7.3-rc，分支 `4.7`，remote 指 `yhyu13/godot` 而非上游。 |

- 事实锚点：`git log` 仅见 `f6ab5db` "Merge pull request #122593 from akien-mga/4.7-disable-physics-nav-3d"（Rémi Verschelde，2026-08-20）。
- 此时仓库根目录**没有** `AGENTS.md`，但有一份未跟踪的 `CLAUDE.md`（79 行，内容准确）。

---

## Era 2 — AGENTS.md 首轮：把构建体系压成可核事实

| ME | YOU |
|---|---|
| 「Create or update `AGENTS.md` for this repository.」并立下两条纪律：「Prefer executable sources of truth over prose.」「When in doubt, omit.」 | 读了 `README`/`pyproject.toml`/`.pre-commit-config.yaml`/`version.py`/`CONTRIBUTING.md`/`SConstruct`/`tests/`，逐条给 `file:line` 锚点，写成 47 行 AGENTS.md。 |

- 写进 AGENTS.md 的高信号事实：SCons 而非 CMake/Make；`target` ∈ editor/template_debug/template_release（`SConstruct:164`）；Windows 默认 MSVC、`use_mingw=yes` 换 MinGW（`detect.py:179-182,227`）；`tests=yes`/`compiledb=yes`/`ninja=yes`/`dev_mode=yes`；`module_<name>_enabled=no`（`SConstruct:485`）；产物命名 `bin/godot.<platform>.<target>[.dev].<arch>` 与 Windows `.console.exe` 双产物（`SConstruct:1034-1051,1157`）；doctest `--test` 过滤（`test_main.cpp:117-123`）；GLSL 用独立 clang-format 配置；架构分层 + servers 模式 + 不碰 `thirdparty/` + 生成文件不手改；改脚本 API 必更新 `doc/classes/*.xml`（`--doctool`）。

---

## Era 3 — 纠错：`platform` 不是「必填」，是「自动探测」

| ME | YOU |
|---|---|
| （未直接指出，但任务隐含「信可执行源」的标准在起作用。） | **先写错了**：首版 AGENTS.md 写「`platform` 必填、缺省即报错」——这是从注入内容里继承的过时说法。随后对拍 `CLAUDE.md` 与 `SConstruct:383-425`，发现 4.7 已改为从宿主 OS 自动探测（win32→windows、darwin→macos、linux→linuxbsd），只有探测不到或跨平台才需显式传 `platform=<p>`。**立刻修正**。 |

- 这是本阶段唯一一次实质纠错，也是「散文说法 vs 可执行源」冲突时以 `SConstruct` 为准的一次落地。
- 教训已固化：**过时的注入内容会以「事实」的面目混进来，交叉比对可执行源是唯一防线。**

---

## Era 4 — OpenWolf 落地：上下文管理系统注入

| ME | YOU |
|---|---|
| 在仓库里初始化了 OpenWolf（`15:54:07`），并让它覆盖 `.kilo/`/`.claude/`/`.cursor/`/`.codex/`/`.opencode/` 五套 agent 配置。 | 排查后确认：`.wolf/` 全套状态（memory/cerebrum/buglog/hippocampus/token-ledger）**全部零初始化**；`AGENTS.md` 与 `CLAUDE.md` 被自动追加了 `<!-- openwolf:begin -->` 引用块；`anatomy.md` 扫描到 513 个文件、0 hits。**诚实发现：OpenWolf 是 web-app 通用模板，与本 C++ 引擎错配**（designqc 截图、reframe UI 迁移、npm audit 都无处可施）。 |

- 事实锚点：`_scan-state.json` `git_head=f6ab5db28…`、`file_count=513`；`hippocampus.json` `total_events=0`；`token-ledger.json` `total_sessions=0`。

---

## Era 5 — AGENTS.md 复验：零改动交付

| ME | YOU |
|---|---|
| 再次下达同一个「Create or update `AGENTS.md`」任务（复验性质）。 | 逐条复验上一轮的全部锚点（`compiledb`/`ninja`/`tests`/`.console.exe`/doctest/GLSL/`--doctool`），确认代码库未变（仍 `f6ab5db`）、AGENTS.md 无漂移、OpenWolf 记忆库为空、无可合并的「项目实学」。**结论：不改，而非为交付而改。** |

- 诚实的「零改动」也是结果：文件已满足「每行都是 agent 不看会踩坑」的标准，再动就是破坏。

---

## Era 6 — 当前：dump journey

| ME | YOU |
|---|---|
| 「dump to journey」——要求把历史沉淀成可复用的 vibe-coding 经验。 | 从 `.wolf/` 记忆库（全空）+ `kilo_local_recall`（仅 1 条会话）+ git（单 commit）重建时间线，如实写成这份 JOURNEY.md。 |

---

## Era 7 — 文档化流水线：144 模块，1 小时 22 分钟跑完

| ME | YOU |
|---|---|
| 「start cron sub agents every 5 mins to doc every single module in godot… use writing skill to write docs in Chinese」；四问后拍板：整个引擎 144 子系统、按层分目录、教学式 edu-doc-writer、调度写进 OpenWolf cron。 | 建 `docs/INDEX.md`（144 条目三态追踪）+ `docs/DOC_SPEC.md`（8 段模板 + 6 纪律）；注册 cron `doc-module-dispatch`；发现 OpenWolf 守护进程没在跑，改用 `task` 后台子 agent 亲自当 dispatcher，14 批并发派发，144 模块全部落盘。 |

- **耗时**：16:56:49 任务下达 → 18:18:51 最后一个模块（`platform/windows`）完成 = **约 1 小时 22 分钟**。其中 setup（调研 + 4 问 + 建追踪/契约/cron + 首批派发）约 18 分钟，执行 14 批 144 模块约 64 分钟，吞吐约 2.25 模块/分钟（每批 7-14 个并发）。
- **故障**：148 次派发里 4 次瞬时故障（1 次 prompt schema 校验失败 + 3 次 connection reset），全部用 `task_id` resume 恢复，零进度丢失。
- **纠偏**：子 agent 反复对拍源码纠正我 prompt 里的错误假设——`MultiplayerReplicator`→`SceneReplicationInterface`、`modules/raycast` 是 Embree 后端不是 RayCast 节点、`modules/svg` 用 ThorVG 不是 nanosvg、`core/input` 无 `InputFilter`……「写前 grep 确认」纪律在 144 个自治 agent 里真实生效。
- **对账**：我发现最初写的「145 / modules 58」是错的，对账文件系统实际是 144 / 57，已修正 `docs/INDEX.md`。

---

## Era 8 — 索引并入 AGENTS.md/CLAUDE.md，push 到远端

| ME | YOU |
|---|---|
| 「这些 doc 做成索引加入到 Agent md 和 claude md」「add files push to remote」 | 把 `docs/INDEX.md` 升级成可点击阅读索引（保留 `- [x]` 标记不破坏流水线）；给 AGENTS.md、CLAUDE.md 各加一节「Module teaching docs (144)」；扫无敏感信息后 `git add -A` → commit `83bd709`（231 文件 +34324 行）→ 推到 `origin/4.7-local`。 |

---

---

## Era 9 — LLM 友好 DSL：从调研到 parser 第一片 red-green

| ME | YOU |
|---|---|
| 「给 godot 设计一个 LLM 友好 DSL… 直接和底层 cpp 沟通… search web subagents」 | 3 后台 agent 并行调研（binding 内部 / DSL 先例 / 性能），写 `doc_ai/GODOT_LLM_DSL_DESIGN.md`（两层：JSON→tscn 声明式 + GDExtension ptrcall 逻辑），37 个 file:line 锚点全部 grep 核实。 |
| 两轮 review（人审 + subagent critic） | 各抓实质错：路线 A 走 GDExtension 不可达、JSON Schema 校验不了 tscn、ptrcall 零校验会 UB、以及「只有两种」过度断言 + 与官方 FAQ 冲突。逐条 grep 复核后才改，没照抄 critic。 |
| 「把这个润色成 skill，还需要 multisubagent 吗」 | 写全局 skill `source-anchored-design`；结论：研究侧多 agent 非默认（门控），critic 侧单 agent 必须保留——值在 critic loop。 |
| 「continue our LLM DSL」（加载 software-dev-loop） | 写 `doc_ai/PLAN_LLM_DSL_IMPL.md` + 自评；问 D1/D2。拍板：D1=独立工具目录快速测试、D2=parser 先行（覆盖我「声明式先行」的建议）。搭 `gdsl/` standalone harness，parser 第一片 red→green + 4 用例 9 断言全绿。 |
| 「1. 你满足什么成功标准？2. 你动了什么没动什么？」（两次审计） | 如实答：只满足 40 行 parser 的 red→green，不是 DSL；动的只有 `gdsl/` + 过程文档，引擎源码/测试/产物/git 零改动；进度 = 6 片第 1 片 1/3。并当场修正 STATUS 里程碑句与 D2 的自相矛盾。 |
| 「update journey… 继续」 | 切片2：`parse_state_field`（`name: type = default`）red→green + float/拒绝边界，9 用例 23 断言全绿。 |

- **事实锚点**：`gdsl/parser.{h,cpp}`、`gdsl/test_parser.cpp`、`gdsl/test.ps1`（MSVC /utf-8 + /EHsc + /std:c++17 + `thirdparty/doctest/doctest.h` 单头）。red = `REQUIRE(ok) is NOT correct! values: false`；green = 切片1 `4 passed / 9 assertions`、切片2 `9 passed / 23 assertions`。
- **自纠**：切片2 写 stub 时误删 `parse_type_decl` 里的 `trim` 行，读文件对拍后当场修复（未等用户发现）；顺手把 `parse_state_field` 的 stub 一起补上。
- **踩坑**：MSVC 默认 GBK(936) 读源码，UTF-8 中文注释让 `parse_type_decl` 声明被误解析（C2039），加 `/utf-8` 修复（buglog bug-001）。
- **决策**：D1 独立工具目录 = 放弃复用引擎 doctest 约定，换秒级 cycle-time（SOP §1.1 的 metric 优先于约定一致性）；D2 parser 先行 = 用户覆盖我的「声明式 tracer bullet」建议，按 §8 执行。

---

## 这个项目如何教 vibe coding with AI

### 人的工作（decide, correct, kill）

- **把纪律写进任务本身**：两次 AGENTS.md 任务都自带「Prefer executable sources of truth over prose」「When in doubt, omit」「improve in place rather than rewriting blindly」——这些不是口头要求，是写死的验收标准，直接决定了我对拍 `SConstruct` 并抓到过时说法（Era 3）。
- **用复验代替追加**：第二次下达同一任务（Era 5）不是为了加内容，而是逼我逐条复查；结果是诚实的「零改动」而不是无意义的增补。

### AI 的工作（instrument, falsify, report honestly）

- **交叉比对可执行源，抓到自己的错**：Era 3 里「`platform` 必填」是我先写进去的错，靠 `CLAUDE.md` vs `SConstruct:383-425` 对拍才推翻。AI 的价值在于敢于报告「我先写错了」。
- **报告不适用，而不是硬套**：Era 4 识别出 OpenWolf 的 web-app 模板与本 C++ 引擎错配，明确说「designqc/reframe/npm audit 无处可施」，而不是假装能用。
- **空就是空**：Era 5 明确「记忆库全空、无项目实学可合并」，不为凑篇幅编造历史。

### 可复用的规则

1. **注入内容 ≠ 事实，可执行源才是**：任何从上下文/模板里顺来的说法，先到 `SConstruct`/`detect.py`/配置里对拍再落笔（Era 3 的 `platform` 纠错）。
2. **逐条给 `file:line` 锚点**：AGENTS.md 每一条都钉在源码行号上，复验时一眼可查、不会漂移（Era 2 → Era 5 全程可复验）。
3. **「零改动」是合法交付**：复验确认无误时，不改比硬改更诚实（Era 5）。
4. **识别模板错配要明说边界**：发现工具/模板与项目类型不符时，报告「不适用」并给出原因，而不是让下个 agent 照做踩坑（Era 4）。
5. **记忆库为空时要直说**：重建历史时 `.wolf/` 全空、`kilo_local_recall` 只有 1 条会话，就写一份诚实的引导期记录，而不是编一段「十时代史诗」（Era 6 全程）。

### 一句话总结

人把「信可执行源、宁可省略」的纪律写进任务并反复验收，AI 负责把散文压成带锚点的事实、在对拍中揪出自己的过时假设、并如实报告「零改动 / 不适用 / 记忆为空」——**人立标准，AI 保真。**

---

## 7×24 自治 agent 的启发（来自 144 模块文档化）

**结论：1 小时 22 分钟跑完 144 个模块（148 次派发、4 次瞬时故障全恢复、零进度丢失）——自治 agent 能 24 小时干活，前提是「契约 + 状态 + 可恢复」三件套，而不是「不会失败」。**

1. **状态机 + 幂等认领，不是定时器**：`[ ]`/`[~]`/`[x]` 三态 + 「先认领再动手」，让 7-14 个并发 agent 不撞车、崩溃后能续。证据：14 批并行无一次重复写同一模块（Era 7）。
2. **瞬时故障是常态，resume 是标配**：148 次派发 4 次失败（2.7%）——1 次 prompt schema、3 次 connection reset，全部 `task_id` resume 恢复。7×24 的 agent 必须把「可恢复」当一等公民，而不是假设每次调用都成功（Era 7 故障记录）。
3. **契约放共享文件，prompt 放最小指令**：`DOC_SPEC.md` + `INDEX.md` 承载全部纪律，每个子 agent 的 prompt 只有 ~15 行指向契约。这是 144 次派发便宜、可扩展的原因（Era 7）。
4. **质量纪律要写死，才能在自治 agent 里传播**：「写前 grep 确认、不编造」是硬规则，于是子 agent 反复纠正我的错误提示（nanosvg→ThorVG、MultiplayerReplicator→SceneReplicationInterface 等）。纪律写进契约才会被执行，停在口号就没用（Era 7 纠偏）。
5. **追踪器要对账地面真值**：自治 agent 的追踪器会漂移——我最初写「145 / modules 58」，实际是 144 / 57，靠 INDEX vs 文件系统对账抓出来。不能只信自己的 tracker（Era 7 对账）。
6. **并行 >> 串行 cron**：按「每 5 分钟 1 个」串行要约 12 小时；14 批并行 64 分钟跑完，约 11 倍加速。7×24 的价值在吞吐，不在「定时」（Era 7 耗时）。
7. **人干「立契约 + 事后核账」，不盯每个 agent**：我的杠杆在前期（追踪器 + 契约 + cron 任务）和事后（对账计数、抽查质量），中间 14 批是 fire-and-forget 等通知（Era 7 全程）。**
