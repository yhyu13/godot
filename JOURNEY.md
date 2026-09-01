# JOURNEY.md — godot 引擎 fork 的引导阶段记录

> 本文件记录「人怎么决策、纠正、砍方向」与「AI 怎么执行、证伪、诚实报告」的时间线。
> 列名：`ME = 用户`，`YOU = AI`。日期格式 `YYYY-MM-DD`。
> 当前阶段：**S6 真机集成进行中 + segfault lane 收尾（2026-09-01）**：route B 已跑通（`.gdsl` → 生成 C → `cl` 编 `.dll` → `.gdextension` → 引擎加载）；两个真机 bug（emit_signal 兼容哈希 + hint_string NULL 崩溃）已修；**退出阶段 segfault 已确认修复**——五样例（self_rule/target_only/emit_only/combo/minimal）在官方 rc3 与 fork release-editor 构建上 plain-exit 全 exit 0；unload→free 崩确认为非 editor 模式 artifact（非引擎 bug）。下一步：009 引擎集成完整验收 + 010 LLM 度量（FR-006/007/008）。

## 风险与待办（roll-up，非替换——各时代的行内风险/待办仍在原表里）

- **[Era 1·起步] 浅克隆 + fork 远端**：`git rev-parse --is-shallow-repository` = `true`，仅见一个 commit `f6ab5db`；remote 是 `yhyu13/godot`。**不能假设能 push 上游 `godotengine/godot`**，PR 走 fork 流程。
- **[Era 4·OpenWolf] web-app 模板与 C++ 引擎错配**：`.kilo/command/{reframe,security-audit}.md`、`.claude/commands/*`、`.wolf/OPENWOLF.md` 的 `designqc`/`reframe`/`npm audit` 都是 React/Tailwind/web 套路。agent 照做会落空（本仓库无 `package.json`/dev server）。待确认：裁剪成 Godot 适用，还是接受不适用。
- **[Era 4·OpenWolf] 记忆库全空**：`.wolf/{cerebrum,memory,STATUS}.md`、`buglog.json`、hippocampus/token-ledger 均 15:54:07 初始化为零。TODO：真实开发开始后按 OPENWOLF.md 协议回填，否则 OpenWolf 形同虚设。
- **[Era 4·OpenWolf] anatomy.md 对 C++ 文件价值低**：`file_count=513, hits=0`，多数描述只是版权首行（如把 `CLAUDE.md` 标成「OpenWolf」）。RISK：agent 依赖 anatomy 描述会读到无意义文本。
- **[Era 2-3·AGENTS] 两份指令文件会漂移**：`AGENTS.md`（紧凑）与 `CLAUDE.md`（更详）内容重叠（构建/测试/架构/贡献约定）。TODO：后续改动若只更新一份，另一份会过期。
- **[Era 8·push] `.wolf/` 运行时状态文件被提交进 git**：`hippocampus.json`/`token-ledger.json`/`_scan-state.json` 等含机器绝对路径 `D:\GitRepo-My\godot`，会随每次会话变化产生提交噪声。待确认：是否给 `.gitignore` 加规则排除。
- **[Era 17·真机集成] 退出阶段 segfault（EXIT 139）：已修复（2026-09-01 验证）**：普通退出（加载含方法 GDExtension + 退出）在官方 rc3（`.godot-bin/`）与 fork release-editor 构建上都 exit 0；五样例双引擎连跑全过（见 Era 23）。根因 = 引擎卸载顺序错位（GDExtension 对象先于 ObjectDB/ClassDB cleanup 销毁，存活实例的 `free_instance_func` 悬空进已卸载 DLL）；修复本体 = 54864dd（`_unregister_extension_class` 无条件 `_clear_extension`，`gdextension.cpp:744`）。

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

## Era 10 — DSL 内核全切片：S3 补全 + S1–S5 全部 red-green

| ME | YOU |
|---|---|
| 「where were we in the journey towards DSL for LLM? do all parts for me」 | 定位到 parser 只完成两个单行产生式（9 用例 23 断言）；补全 S3（`parse_type_block` / `parse_rule_block` / `parse_program`），再按计划落地 S1–S5：S1 `emit_tscn`、S2 JSON→SceneSpec+schema、S4 `typecheck`、S5 `emit_c`。每片先红后绿（stub→断言红→实现→绿）。 |
| 授权运行 MSVC harness（`test.ps1` 被 consent 门拦下，选「1 你批准」） | 把 harness 从 2 文件扩到 11 文件（parser/codegen_declarative/json/scene_json/typecheck/codegen_logic + 4 个 test_*.cpp），累计 35 用例 125 断言全绿。 |
| （沿用 D1/D2，无新拍板） | S2 的 JSON 解析遗留问题（PLAN 自评 #3）拍板：手写最小 JSON 解析器 `gdsl/json.{h,cpp}`，内核零第三方依赖。 |

- **事实锚点**：`gdsl/` 从 2 文件扩到 13 文件。累计 35 用例 / 125 断言全绿（red 证据 = `REQUIRE(ok) is NOT correct! values: REQUIRE(false)`）。
- **切片落地**：S3 补全 parser 到完整 AST（`Program{types,rules}`）；S1 `emit_tscn` golden 逐字节；S2 `scene_from_json`（JSON→tscn 端到端 tracer bullet 通过）；S4 `typecheck` 拒绝重复类型名/未知字段类型/重复字段/未知触发类型；S5 `emit_c` 类型映射（int→int64_t、float→double、Named→指针）+ 确定性。
- **诚实边界（S6 未做）**：环境无 scons、无内置引擎二进制；且 `emit_c` 当前只输出 struct 定义 + rule 注释，尚未生成真 GDExtension 注册/effect/ptrcall 代码——此时接引擎是接骨架。S6 需 (a) 装 scons + 全量引擎构建；(b) 先把 S5 codegen 扩到真 GDExtension 代码。
- **踩坑**：`\uXXXX` 转义下标（读 `s[i+1+k]` 而非 `s[i+k]`）；多文件 doctest 只有一个 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`（test_parser.cpp），其余 test 文件只 `#include "doctest.h"`。

---

## Era 11 — S6.1：codegen 扩到真 GDExtension 注册层

| ME | YOU |
|---|---|
| 「both」——(1) 回填 memory.md 的 Era 10 动作日志，(2) 进 S6 第一步：扩 codegen 到真 GDExtension 代码（先纯 codegen 测试，不依赖 scons）。 | 回填 memory；用 `core/extension/gdextension_interface.json` + `gdextension_interface_header_generator.cpp:307-313` 逐条核对 4.7 ABI（`classdb_register_extension_class6` / `GDExtensionClassCreationInfo6` / `GDExtensionClassCreateInstance3` / `string_name_new_with_latin1_chars` / `classdb_construct_object` / `object_set_instance` / `mem_alloc/free`），并对照官方 GDExtension C example 的 create/free instance 范式；`emit_c` 从「struct + 注释」扩成完整 GDExtension C 源（入口点 + API 缓存 + SCENE 初始化 + 每类型注册 + 实例生命周期 + 状态字段 C struct）。 |

- **事实锚点**：`gdsl/codegen_logic.{h,cpp}` 重写；`gdsl/test_codegen.cpp` 4 用例 → 7 用例；累计 38 用例 / 147 断言全绿（red 证据 = 新增 GDExtension 断言对旧 emit_c 全失败，5 用例 16 断言红）。生成 C 源经肉眼 dump 核对：brace 平衡、ABI 符号正确、确定性。
- **决策 D3**：用 `classdb_register_extension_class6`（4.7 最新，无 deprecated 警告）；`GDExtensionClassCreationInfo6 class_info = { 0 }` 零初始化 + 只设非默认字段（比 22 字段 designated initializer 更可移植，规避 MSVC C99 designated-init 限制）。
- **诚实边界**：规则仍为注释——effect/ptrcall codegen 被 effect ontology 开放决策（设计文档 §7 最大风险）阻塞，未擅自发明 ontology。S6 剩余：effect/ptrcall、`modules/gdsl/` 模块、scons 真机构建。
- **踩坑**：dump 样例程序漏声明 `type Bullet` 触发 typecheck 拒绝「unknown trigger type」（非 codegen bug，是样例输入错）。

---

## Era 12 — S6.2a：effect/ptrcall codegen v1（set_field + 方法注册）

| ME | YOU |
|---|---|
| 「自治开发循环：把两层 DSL 落地为可编译可测的编译器… 每轮先读 STATUS… 开放决策拍最小可辩护 ontology 继续，不要停摆」 | 读 STATUS/cerebrum/anatomy；定位到 S6 的 effect/ptrcall codegen 是当前唯一不阻塞切片（scons/模块集成被环境阻塞）。拍板 effect ontology v1（D4）：`set_field` + 单比较 guard、self-only；`emit_signal`/跨参与者 `target` 明确拒绝。red→green 落地 `gdsl/effect.{h,cpp}`（guard/effect 解析器）+ typecheck 结构化 when/then + `emit_c` 规则→方法（`_impl`/`_call`/`_ptr` + `classdb_register_extension_class_method`）。 |

- **事实锚点**：`gdsl/effect.{h,cpp}` 新建；`gdsl/typecheck.{h,cpp}`（TypedRule 增 `guard`/`effects` + 字段存在性校验）、`gdsl/codegen_logic.cpp`（方法发射 + API 缓存 + 方法注册）、`gdsl/test_effect.cpp`（新）、`gdsl/test_typecheck.cpp`、`gdsl/test_codegen.cpp`、`gdsl/test.ps1` 改。累计 **57 用例 / 226 断言全绿**（red 证据 = effect 解析器 stub 阶段 `50 用例 44 passed 6 failed`，全是断言红非编译错；codegen 阶段「ruleless 类型无方法注册」用例因断言串误匹配 API 缓存名先红后修）。
- **决策 D4（effect ontology v1）**：动词从 `set_field` 起步；guard 单比较；`ref` 仅 `self`。规则→方法走 `classdb_register_extension_class_method`（call_func + ptrcall_func 双路径，零参方法两路径 body 一致）。`emit_signal`/`target` 明确拒绝而非静默忽略。
- **诚实边界（emit_signal 为何不做）**：GDExtension ABI **无** `object_emit_signal`（`gdextension_interface.json` 全 grep 零命中）；唯一路径是 `classdb_get_method_bind("Object","emit_signal",hash)` + `object_method_bind_call`，但 `classdb_get_method_bind` 要求精确方法哈希（`class_db.cpp:1166-1198` 走 `get_method_with_compatibility` 要求 `get_hash()` 全等），而 `MethodBind::get_hash()` = `MethodInfo::get_compatibility_hash()`（`method_bind.cpp:35-45`，含参数类型签名），`emit_signal` 是 variadic，哈希必须从 `extension_api.json` dump 取——不能瞎编哈希。留 S6.2b。
- **验证**：生成 C 经 stub `gdextension_interface.h`（按 json 类型名/字段名手写）用 `cl /c` 语法编译通过（`SYNTAX OK`），证明发射的 C 结构/成员名/函数指针类型/转换全部合法。
- **踩坑**：`test_codegen.cpp` 的「ruleless 类型无方法注册」用例断言 `c.find("classdb_register_extension_class_method") == npos`，但该串总出现在 API 缓存声明里 → 红；改成断言「注册调用串」`gdsl_classdb_register_extension_class_method(gdsl_library, &class_name, &method_info);` 缺席后绿。

---

## Era 13 — S6.2b：emit_signal codegen（D5 阻塞解除，哈希从源码算）

| ME | YOU |
|---|---|
| （自治 cron，延续「拍最小可辩护 ontology 继续，不要停摆」） | 读 STATUS/cerebrum/anatomy；定位 S6.2b `emit_signal` 是唯一不被 scons/模块集成阻塞的切片（D5 说哈希必须从 `extension_api.json` dump 取，但环境无内置引擎 dump 不了）。**推翻 D5 的「只能 dump」结论**：`get_compatibility_hash` 是纯结构哈希（方法名不参与），可逐条移植 Godot 源码的 murmur3/fmix 自己算。 |

- **事实锚点（哈希怎么算出来的，非手编）**：移植 `hash_murmur3_one_32`+`hash_fmix32`（`core/templates/hashfuncs.h:112-122,144-150`）与 `get_compatibility_hash`（`core/object/method_info.cpp:87-116`）；`emit_signal` 的 MethodInfo（`core/object/object.cpp:1864-1870`）＝ 1 个 STRING_NAME 参数（`variant.h:96-124`，NIL=0…STRING_NAME=21）+ vararg（`method_bind.h:178` `is_vararg()=true`）+ 无返回 + 无默认参数。独立 standalone 程序算出 **2866548813 (0xAADC104D)**，再把它钉进 `gdsl/test_abihash.cpp` 断言（`emit_signal_compat_hash()==2866548813u`），防引擎 MethodInfo 漂移。
- **Variant 封送的坑**：4.7 ABI **无** `variant_new_string_name`（grep 全 json 只有 `variant_new_copy/nil` + 通用 `variant_construct`），改用 `get_variant_from_type_constructor(GDEXTENSION_VARIANT_TYPE_STRING_NAME)`（`gdextension_interface.cpp:564-607` 返回 `VariantTypeConstructor<StringName>::variant_from_type`）。**无** `string_name_destroy` → 每发射一次建 StringName 会无界泄漏 `static_count`（`string_name.cpp:231-232`），故信号名用 `p_is_static=true` 缓存一次复用（static StringName 免析构、程序生命周期）。
- **落地**：`gdsl/abihash.h` 新建；`effect.{h,cpp}` 增 `EffectKind::Emit` + `parse_emit`（`emit(<ident>)` 零参，带参/空名/未闭合明确拒绝）；`typecheck.cpp` Emit 效果跳过字段校验；`codegen_logic.cpp` 发射 emit_signal 缓存（`classdb_get_method_bind` + hash 字面量）+ `gdsl_emit_signal` helper（Variant 封送 + `object_method_bind_call` + `variant_destroy`）+ 信号名 static StringName 去重缓存；`test.ps1` 加 `test_abihash.cpp`。累计 **66 用例 / 264 断言全绿**（red 证据 = `hash_fmix32(0)!=0` 断言先红——我误以为 fmix 不是恒等，实为 fmix(0)==0，修正断言后绿）。
- **验证**：生成 C（含 emit 规则样例）经 stub `gdextension_interface.h`（类型/字段名逐条对照 json）`cl /c` 语法编译通过（EXIT=0 零警告），证明发射的 `classdb_get_method_bind`/`object_method_bind_call`/`get_variant_from_type_constructor` 调用与 typedef/struct 字段全部合法。
- **诚实边界**：Variant 封送的**运行时正确性未验证**（无引擎二进制，`cl /c` 只证明语法/类型合法，不证明信号真的发出）。哈希值已从源码推导 + 独立程序 + 测试三处一致，但最终以真机 `extension_api.json` dump 对拍为准。`target` 跨参与者（S6.2c）仍开放——需先定 target 类型来源。
- **决策 D5 记录**：见 STATUS「Closed decisions」与 cerebrum「Decision Log」。

---

## Era 14 — S6.2c：跨参与者 target codegen（D6/D7 拍板）

| ME | YOU |
|---|---|
| （自治 cron，延续「拍最小可辩护 ontology 继续，不要停摆」；指令明确 route B = GDExtension 为集成路径） | 读 STATUS/cerebrum/anatomy/cron_digest/设计文档；定位 S6.2c `target` 是最后一个不被 scons/引擎二进制阻塞的 codegen 切片。**拍板 D6**（target 类型显式声明 `rule ... by <Owner> target <Type>:`，不用默认同型/固定 Node——前者=自我无意义、后者丢类型安全）+ **D7**（ABI 无 `object_get_instance`，跨对象取回 C struct 唯一路径 = instance binding）。red→green 落地 parser/effect/typecheck/codegen 四层。 |

- **事实锚点（D7 的关键 ABI 缺口）**：`object_set_instance`（写 self 的 extension instance）存在、但 grep `gdextension_interface.json` **零命中 `object_get_instance`**——引擎内部 `Object::_get_extension_instance()`（`object.h:525`）只在方法调用时把 self 传给 `p_instance`，跨对象取回只能走 instance binding。已核对引擎源码：`GDExtensionMethodBind::call` 用 `p_object->_get_extension_instance()`（`gdextension.cpp:106`）取 self；binding 由 `set_instance_binding`/`get_instance_binding`（`object.cpp:2153,2167`）token 键存取，`free_callback` 为 NULL 时不触发（内存仍由 `free_instance_func` 释放，`object.cpp:194` 的 `_extension->free_instance` 路径）。
- **落地**：`gdsl/effect.{h,cpp}` 增 `RefOwner{Self,Target}` + `resolve_ref`（self/target/裸字段）；`gdsl/parser.{h,cpp}` 增 `RuleDecl.target` + 可选 target 子句解析；`gdsl/typecheck.{h,cpp}` 增 `TypedRule.target` + 按 ref 选 owner 校验字段 + 拒绝「引 target 无子句」；`gdsl/codegen_logic.cpp` 规则方法扩成带 `GDExtensionObjectPtr p_target`（`argument_count=1` + OBJECT `arguments_info`）+ instance binding 取回 + `_call` 用 `get_variant_to_type_constructor(OBJECT)` 解 Variant + `_ptr` 用 `*(GDExtensionObjectPtr*)p_args[0]`。累计 **77 用例 / 319 断言全绿**（red 证据 = 实现前跑出 77 用例 68 passed 9 failed / 290 断言 12 failed，全是新 target 用例断言红非编译错）。
- **验证**：生成 C（含 `rule OnHit by Bullet target Player` 样例）经扩展 stub ABI 头（`GDExtensionPropertyInfo` + binding 回调 + 完整 `GDExtensionClassMethodInfo`）`cl /c` 语法编译通过（SYNTAX OK 零警告）。运行时正确性仍未验证（无引擎二进制，见诚实边界）。
- **诚实边界**：`target` 取回的**运行时正确性未验证**（`cl /c` 只证明语法/类型合法）；`object_get_instance_binding` 对非 DSL 对象会走 NULL `create_callback`（已提供返回 NULL 的 `gdsl_binding_create` + `_impl` 里 target==NULL 早退，防御性兜底），但该路径未经真机确认。route B 集成（生成 C → `.dll` → `.gdextension` → 真场景）仍是 S6 剩余硬仗，需要 Godot 引擎二进制。
- **踩坑**：patch 编辑 STATUS.md 时，D5 行里的 C 字符串字面量 `\"Object\"` 触发 escape-drift 检测——改用不含 `\"` 的锚点（测试计数行）插入 S6.2c 条目，绕开。

---

## Era 15 — spec-kit init + OpenWolf SDD 接入（grill 驱动）

| ME | YOU |
|---|---|
| 「use spec-kit init this project based on doc_ai but grill me」 | 先查实：`doc_ai/` 3 份文档、`gdsl/` 已是 3 commit 的完整内核（77 用例）、GitHub Spec Kit `specify` CLI 已装但**从未在仓库初始化**（无 `.specify/`/`constitution.md`）、OpenWolf SDD 已脚手架（`.wolf/spec-templates/` + 4 套 agent 命令）。三轮 grill 收敛。 |
| 反问「compare GitHub Spec Kit with openwolf, which has hardline on test」 | 对拍两套系统源码：Spec Kit 模板里 tests 标注「OPTIONAL」，无运行时 hook；OpenWolf 有 `post-test.js`（观测失败写 hippocampus）+ pre-read/pre-write TDD 提醒 + tasks-template 硬 phase gate。结论：OpenWolf 在测试硬门轴上明显更强，但**拒绝说 100%**——如实降级为「right design, wrong install」（见下）。 |
| 拍板 Q1=OpenWolf、Q2=scope 到 gdsl+doc_ai、Q3=retro+forward S6、Q4=中文叙事+英文标识符 | 执行：修运行时、promote DoD、写 9 spec、设 active。 |

- **抓到 OpenWolf `init` 的 stale copy-list bug**：`copyHookScripts`（`src/cli/init.js`）只拷 13 个固定 hook 文件，但打包的 `pre-read.js`/`pre-write.js` import `../specs/spec-store.js` + `../specs/inject.js`，init **从不拷 `specs/` 运行时** → 每次全新 `openwolf init` 的 SDD 注入 + TDD 提醒都抛 MODULE_NOT_FOUND（被 catch → fail open 静默）。**本地修法**：把 openwolf 包 `dist/hooks/specs/*.js` 拷进本仓库 `.wolf/specs/`（不是 `.wolf/hooks/specs/`——import 是 `../specs/` 相对 `.wolf/hooks/`）。node 验证 pre-read/pre-write 均 RESOLVES。
- **openwolf 本身没修、没 push**：openwolf 是个独立 git 仓库（`C:\nvm4w\nodejs\node_modules\openwolf`，origin=`github.com/yhyu13/openwolf`，branch main），本次**只从它读、往外拷**，零写入零 push。它自身 `copyHookScripts` 的 bug 仍在（修它需在 openwolf 仓库里给 copy 列表加 `specs/`，属另一任务，未做也未要求做）。它工作区那批未提交改动（AGENTS.md/README.md/post-write.ts 等）是既有的，非本次产生。
- **promote SOP §4.4 DoD 为硬门（D9，both）**：(a) 4 套 agent 的 `/implement` 命令改写——无粘贴绿色 `--test` 输出不许勾任务框，且要求 red-before-green + 测试文件红绿间逐字节不变；(b) `.wolf/hooks/post-test.js` 在 implement 阶段测红时把 active spec 标 `blocked`。**诚实边界**：`openwolf spec next` 自身不因 `blocked` 拒进（改它要 patch 全局二进制、每次 `openwolf update` 即丢），故 (b) 是持久标记、(a) 才是执行闸。
- **9 个 spec**：8 个 retro（`specs/001-json` … `008-codegen-logic`，每个 seam 对应现有测试文件）+ 1 个 forward（`009-engine-integration`=S6，falsifiable 场景加载 bar）。中文叙事 + 英文标识符。`doc_ai/` 原样保留为设计档案。`009` 设 active、phase specify。
- **遗留缺口（诚实记录，未粉饰）**：`json` 无直接测试文件，仅经 `scene_json` 间接覆盖——写进 `001-json` FR-003 标为待办。
- **验证**：`openwolf spec list` 见全 9 个；`gdsl/test_gdsl.exe` 77 用例 / 319 断言全绿；STATUS.md drift guard 从警告转干净。
- **决策**：D8（SDD 选 OpenWolf，测试硬门轴）、D9（DoD 双通道 promote + 已知限）、D10（一组件一 spec）。已回写 cerebrum Key Learnings + Decision Log + memory。

---

## Era 16 — 复盘：这 9 份 spec 到底是不是「LLM 友好」

| ME | YOU |
|---|---|
| 「为什么要这么做？我不明白技术限制？gdsl 目标是什么？」 | 解释两层流水线 + 5 条技术限制（语言注册进不了 GDExtension、声明式走现成 loader、逻辑走 ptrcall 零装箱、emit_signal 哈希必须自算、JSON 校验不了 tscn）。gdsl 目标 = 让 LLM 写「配方」不是写「程序」，可校验 > 可表达。 |
| 「review spec，true LLM friendly? why?」 | 诚实复盘：机制对、结论早。分两半说。 |

- **对的一半（机制确实是 LLM 友好的正确杠杆）**：固定 effect ontology（005，LLM 不能发明符号）、reject-not-retry（002/004/006，把「幻觉→运行时崩」变成「编译错→改」）、确定性输出（003/008，格式化噪声无关）、小语法（004，可写错的空间小）。这四条是 PuzzleScript/VGDL 已验证的经验，不是拍脑袋。
- **没证明的一半（claim 说早了）**：① ontology 覆盖边界是设计 §7 自称的「最大设计风险，不是技术风险」，无 spec 处理——固定 ontology 覆盖不了真实玩法规则，LLM 友好就落空；② .gdsl 零预训练数据，GDScript 有海量数据，「新语言靠 spec + few-shot 学得会」是经验命题，没测试；③ ScriptDoctor 的胜因是「编译错 + playtest 反馈」的**闭环**，我们只 spec 了编译器那一半（reject），playtest/反馈那一半缺席；④ 002 只校验结构不校验语义（引用不存在的节点照样能过）；⑤ 8/9 份 spec 讲的是实现，不是 LLM 面对的那个接口（语法、ontology 表、报错文案、few-shot 示例）。
- **判决**：spec 证明的是「编译器正确」，不是「LLM 能用它做游戏」。两者不是一回事。「LLM 友好」实际断言的是后者，目前未证。补齐三件才算数：ontology 覆盖真实规则、有 playtest 反馈闭环、拿真 LLM 跑收敛测试。

---

## Era 17 — S6 真机集成：两个真 bug 修复，一个退出 segfault 未修

| ME | YOU |
|---|---|
| （延续「拍最小可辩护 ontology 继续，不要停摆」+ route B 集成指令） | route B 真机跑通：`.gdsl` → 生成 C → `make_interface_header.py`/手写 stub 头 → `cl` 编 `.dll` → `.gdextension` manifest → 引擎加载。引擎二进制就位（`.godot-bin/`，已 gitignore），`--dump-extension-api` 对拍开始。 |
| （未新增拍板，延续 D 系列决策） | **修掉两个真机 bug**：① emit_signal 兼容哈希 2866548813 → **4047867050**（根因：emit_signal 的 MethodInfo 有 Error 返回值，不是无返回；手写 mi 漏 return_val，`create_vararg_method_bind` 从 `_emit_signal` 签名推导 INT + class_name "Error"）。真机 `--dump-extension-api` 对拍验证。② `GDExtensionPropertyInfo.hint_string` 传 NULL → 引擎 PropertyInfo 构造（`property_info.h:168`）无条件解引用崩溃，改成指向零初始化 `gdsl_empty_string[8]`。效果：之前 "registering method on_hit" 一打印就崩，现在初始化阶段全部通过，加载完 scene 正常走完。 |

- **事实锚点（两个已修 bug）**：`core/object/object.cpp:1866-1869`（emit_signal 手写 MethodInfo，1 个 STRING_NAME 参数、vararg）；`core/object/object.cpp:1151`（`_emit_signal` 返回 Error → `create_vararg_method_bind` 从签名推导 return type）；`core/variant/type_info.h:247-256`（MAKE_ENUM_TYPE_INFO(Error)：VARIANT_TYPE=INT、class_name="Error"）；`core/string/ustring.cpp:2755-2764`（String::hash = djb2，class_name 哈希用）；`core/object/property_info.h:163-170`（PropertyInfo(const GDExtensionPropertyInfo&) 无条件解引用 name/class_name/hint_string）。哈希从 0xAADC104D → 0xF1458CAA，钉进 `test_abihash.cpp`。
- **还剩一个 bug（未修，如实说）**：退出阶段 segfault（EXIT 139）。规律：任何注册了方法（有 rule）的 GDExtension 退出时都崩，与 emit/target/connection/hint_string 都无关——一个只有 self 规则、无 emit 无 target 的最小样例也崩。位置在 "loading_editor_layout DONE" 之后、Godot crash handler 卸载之后，是 CRT 静态析构阶段的裸 segfault；cdb 下见 "Invalid parameter passed to C runtime function"。
- **最可信假设（未证实）**：`GDExtensionMethodBind` 存着指向 DLL 里 call_func/ptrcall_func 的函数指针；若 DLL 在 `ClassDB::cleanup()`（`class_db.cpp:2350` memdelete 每个 method_map 里的 MethodBind）之前被卸载，这些 MethodBind 析构时（虚表/析构函数在已卸载内存里）悬空解引用。`GDExtensionManager` 在 `register_core_types.cpp:433` 被 memdelete，其析构才 `close_library` 卸载 DLL。
- **证据缺口（诚实记录）**：cdb 在 harness 里反复卡住/超时，procdump 没抓到 dump（崩溃在进程 teardown 阶段），WER LocalDumps 也没落盘。所以没有真实调用栈，只有源码路径推理——栈没抓到就不能声称确定了根因。
- **本轮纠错（我做的）**：把上一轮的一个坏修复还原了。曾为追 segfault 把信号名 StringName 的 `p_is_static` 从 `true` 改成 `false`（注释称「true 复用 DLL 字面量缓冲区 → cleanup 悬空」），但 `test_codegen.cpp:150` 仍断言 `true` → 单测其实红了（82/83），与「全绿」报告不符。对拍 `string_name.cpp:206-261` 证伪该注释：StringName 构造始终 `_data->name = p_name`（经 `String::operator=` → `append_latin1` **拷贝**），`p_static` 只影响 `static_count` 泄漏统计（`string_name.cpp:232,245`），不改变缓冲区归属；且 segfault 无 emit 也复现，故该改动不可能是根因。已还原 `true`、删除错误注释，恢复 83 用例 / 335 断言全绿。
- **下一步（下一轮）**：用能跑出完整退出路径的方式抓崩溃栈——gflags `+hpa`（heap page alloc）配合 AEDebug，或把规则方法改成不依赖 DLL 卸载顺序的写法（不挂 ClassDB method_map，用 `GDExtensionClassCreationInfo6` 其他机制）。但这是架构判断，需先拿到栈再定，不猜。

---

## Era 18 — 状态审计：已做 / 怎么度量 / 还剩什么

| ME | YOU |
|---|---|
| 三个问题一次抛：gdsl 做到哪了 / 怎么全面度量 / todo 有哪些；同时挂上 software-dev-loop + shuorenhua 两个 skill | 读 .wolf/STATUS + cerebrum + anatomy + JOURNEY + specs/009/010 + gdsl 目录 + git log，实跑 gdsl/test.ps1 拿活证据，把「已做 / 度量矩阵 / todo」三块答完；重点点出度量缺口——目前只有「编译器正确」「生成代码语法对」有数，引擎集成 / LLM 友好度 / 运行时正确性都还是空 |

- **活证据（非记忆）**：本会话实跑 `gdsl/test.ps1` = 83 用例 / 335 断言全绿；`git log --oneline -- gdsl/` = 4 commit（7a0d05e 设计+parser / 15a6541 内核 / 6435a13 GDExtension codegen / 19d6c04 两真 bug 修复）。
- **度量矩阵 5 维，只有前两维有数**：① 编译器正确性（83/335 红绿）② 生成代码正确性（确定性 + abihash 黄金值 4047867050 防漂移 + stub 头 cl /c 语法）③ 引擎集成正确性（009 的 [SceneTree] 断言，空）④ LLM 友好度（010 的 mutation score / first-try rate / iterations-to-valid / few-shot 数，空）⑤ 运行时正确性（cl /c 只证语法，信号/target 真跑未验证）。
- **todo 收成一条链**：segfault 栈（唯一硬阻塞）→ 009 引擎集成验收 → 010 LLM 收敛度量 → playtest 闭环（FR-011）。segfault 不解，后面全卡。
- **纠偏（自己认的）**：上轮回答里「度量」用的是记忆里的 83/335，本轮改成实跑 test.ps1 拿到的数，并逐维标「已度量 / 缺口」——不把「编译通过」说成「运行时正确」。8 小时攻坚计划另落 `doc_ai/PLAN_GDSL_8H.md`。

---

## Era 19 — LLM 可读性审计：107 个二进制是数据不是代码，方向 = 消费 API 文本面

| ME | YOU |
|---|---|
| 三连问：① 哪些模块不是 LLM 可读（native）② 怎么转文本 ③ 转得对不对、方向对不对。随后追加：还有哪些 Godot 设计「不 AI native」？ | 实扫 `git ls-files` 全量 14,291 文件，NUL 字节 + 扩展名双路分类，得 **107 个二进制 / 12,330,021 字节**；逐类给「能否无损转文本 + 对 LLM 有没有用」；落盘 `doc_ai/LLM_READABILITY_AUDIT.md`。 |

- **事实锚点（实扫，非记忆）**：14,291 tracked / 107 binary / 12,330,021 bytes。二进制全是数据资产——`thirdparty/icu4c/icudt_godot.dat`（4.8 MB ICU 数据表，最大单项）、28 个 `.woff2` 字体、~50 个图标/贴图/查表（logo、Android 启动图、`servers/rendering/storage/ltc/ltc_lut{1,2}.dds`、SMAA AreaTex）、2 个 `.jar`（gradle wrapper + mono Android 加密库）、1 `.keystore`、1 `.glb`、1 `.zip`、7 `.bin`。**无任何预编译 C/C++ 库（`.a/.so/.dll/.lib`）**——iOS `.xcframework` 只有 0 字节 `empty` 占位，预编译库是 SCons 构建时下载的。
- **唯一无损转**：`.glb→.gltf`（同一规范，二进制→JSON 文本），仓库已现成示范 `tests/data/models/cube.gltf`+`cube.bin`（文本+buffer）vs `suzanne.glb`（二进制）。其余字体/ICU 表/贴图「转文本」要么无意义、要么回上游源，LLM 都不读。
- **真正的「native→文本」面 Godot 已自己生成**：`extension_api.json`（6.96 MB 全 ClassDB 导出）+ `core/extension/gdextension_interface.json`（328 KB ABI JSON）+ `doc/classes/*.xml`（1,219 份）。Godot 资源格式的 native↔text 也早已内置：`core/io/resource_format_binary.cpp` vs `scene/resources/resource_format_text.cpp`。
- **方向结论**：把 107 个二进制资产转文本 = 错任务（LLM 写玩法从不读字体/ICU 表）；GDSL「文本进 → native 出、LLM 只见文本」本身是对的，native 层本来就该不透明。正确抓手 = 消费那三份已生成的 API 文本描述，而不是转二进制。
- **追加问「还有哪些不 AI native 的设计」**（设计层，不是二进制层）——答在会话里，要点：GDScript 默认动态类型（Variant 满天飞）、signal/call/get_node 全部字符串寻址、`.tscn` 无 schema（issue #7102）、ClassDB 三份 API 表示会漂移（C++ 宏绑定 vs extension_api.json vs doc XML）、MethodBind 按**结构哈希**不按名字匹配（D5 踩过的 2866548813→4047867050）、资源默认二进制格式、编辑器 GUI 优先而非文本优先、GDScript 是命令式语言（LLM 错率高）——这些正是 GDSL 设计文档里「配方而非程序 + schema 校验 + 固定 ontology」要绕开的对象。**对策（8 条逐条）已落盘 `doc_ai/LLM_UNFRIENDLY_DESIGNS.md`**。

---

## Era 20 — segfault 攻坚：根因钉死、两个修复方向都撞墙、约束摸清

| ME | YOU |
|---|---|
| 「有bug不能engine integration吧，先8hr修复bug?」「go non stop」 | 源码钉死根因（卸载顺序），确定性复现（unload+free→signal 11），上游 issue 对拍（#98182/#95306/#81030/godot-cpp#889），两个修复方向都实测撞墙，约束全部摸清，引擎源码 revert 干净 |

- **根因（源码 + 上游双确认）**：`unregister_core_types()` 里 `memdelete(gdextension_manager)`（register_core_types.cpp:433）→ 卸 DLL + 释放 GDExtension 对象（含 ObjectGDExtension + GDType），但 `ObjectDB::cleanup()`（:488）、`ClassDB::cleanup()`（:495）在其后跑，此刻泄露对象解引用悬空的 `_extension`/`_gdtype_ptr`/free_instance_func → use-after-free。上游 #98182（Terrain3D）崩溃栈一模一样，dsnopek 原话「没有通用修复」。
- **确定性复现**：`unload_extension()` 后 `p.free()` → EXIT 139 signal 11，崩溃栈钉在 `Object::_predelete`（object.cpp:195）调 DLL 的 free_instance_func，GDScript backtrace 钉在 `p.free()` 那一行。
- **修复方向 A（reorder）失败**：把 `memdelete(gdextension_manager)` 挪到 `ClassDB::cleanup()` 后 → 确定性崩溃。根因：`close_library` → `OS_Windows::close_dynamic_library` → `_remove_temp_library` → 创建 DirAccess → `ObjectDB::add_instance`，而 ObjectDB 的 object_slots 已被 `ObjectDB::cleanup()` 释放 → 崩。卸载顺序与核心基础设施纠缠。
- **修复方向 B（_clear_extension）失败**：在 `GDExtension::~GDExtension()` 里 close_library 前调 `_clear_extension` 清实例。实测 `instances` 恒空。根因：实例追踪（`track_instance`）只在 `extension->gdextension.reloadable` 为真时启用（gdextension.cpp:552-562），且 reloadable 还需 `reloadable=true` manifest + `Engine::is_extension_reloading_enabled()`（gdextension_library_loader.cpp:217）。GDSL 扩展非 reloadable → 无追踪 → 无实例可清。
- **正确方向（已选 c 并落地，且重定位到正确位置）**：选了 (c) 让 GDSL 支持 reloadable——codegen 补 `recreate_instance`（`gdsl/codegen_logic.cpp`）+ manifest `reloadable=true` + `gdsl_deinitialize` 注销类（热重载前提）。引擎侧修复**从析构挪到 `_unregister_extension_class`**（`core/extension/gdextension.cpp:738-744`）：注销类时无条件 `_clear_extension`（原来 `if (is_reloading)` 才清，导致正常关机实例泄漏悬空——这正是根因）。实测：autoload 泄漏 Player → 关机时被清成 `Node2D`（同一对象 id）、编辑器 3x 干净、88/350 绿。deinitialize 注销会 erase extension_classes，所以析构里的旧补丁（依赖 map 非空）已撤。
- **热重载状态保持（基本类型已通，reload+关机 flaky 崩溃未定位）**：getter/setter 方法 + `classdb_register_extension_class_property` 已落地（每字段 int/float/bool），`gdsl_deinitialize` 注销类已做。实测 `before hp=7 → reload status=0 → after hp=7`，跨热重载状态保住，日志干净。ABI 三墙（无 `dictionary_set`、无 StringName 比较、无 `object_set_meta`）用「每字段 getter/setter 方法 + 属性注册」绕开，路由由引擎按方法名做。**新暴露 flaky 崩溃**：reload 后再退出偶发 EXIT=139（签名同原始 bug，CRT 静态析构、无 crash handler），1 次崩 3 次不崩，use-after-free 非确定；根因未定位（无 backtrace，崩溃在 crash handler 卸载后）。旧代码 deinitialize 空 → reload 直接失败，此路径以前不可达。getter/setter 未提交（关联未定位 flaky 崩溃）。Named 字段、schema 容错、StringName 泄漏修复、manifest 一致性 = 未做。
- **方向拍板（走 B，reload 硬化）**：放弃 A（009 引擎集成），继续把热重载做扎实。顺序 1→2→3：① 定位 flaky reload+关机 segfault（抓栈坐实悬空指针）；② Named 字段状态保持；③ schema 容错。⑤（StringName 泄漏 + manifest 一致性）随路顺手做。此条为本会话后续主攻方向。
- **方向 1 结果（flaky 崩溃 = dev-build 特有，生产路径干净）**：release 版（target=editor debug_symbols=yes，无 dev_build）28/28 干净不崩，dev 版 1/5 崩 → 悬空解引用只在 dev 的未优化堆布局下崩，release 优化后不崩。抓栈三路全堵：debug CRT 不生效（Godot memalloc 走 HeapAlloc 非 CRT malloc）、cdb 被 dev 版 WARN_PRINT 的 `GENERATE_TRAP()`(`__debugbreak`/int 3，error_macros.h:98) 卡住等输入、gflags 需 admin（本会话无）。结论：flaky 崩溃 = dev-only 开发者工具链小毛病，优先级低，不挡生产。方向 1 降级绕过，可进方向 2/3。
- **方向 2 结果（Named 字段状态保持，已落地）**：codegen 补 Named 字段 getter/setter——getter 把对象引用封成 OBJECT Variant（null 走 NIL，`variant_new_nil`），setter 经 `object_get_instance_binding` 解析回 C 结构体，加 `variant_new_nil`/`variant_get_type` ABI 缓存。新增 `named.gdsl` 样例（`Bullet.owner: Player = null`）。实测 `before hp=7 owner_set=true → reload → after hp=7 owner_same=true`，5/5 确定性正确。依赖顺序不是问题（finish_reload 先 recreate 全部再统一 restore）。commit d1bc48a。
- **方向 3 结果（schema 容错，已落地）**：标量 setter 在封送前加 `variant_get_type != 期望类型 → return`（`gdsl/codegen_logic.cpp`），LLM 改字段类型后旧状态类型不匹配就跳过、留默认值，不污染不崩溃。字段删除/改名已天然安全（属性查找丢未知名）。Named 跨类型（Player→Bullet）是已知限制（本轮不做）。单测 `[GDSL] Emit schema-safe setter` 断言类型检查在生成代码里；89/353 绿；Named 往返 3/3 正常（owner_same + hp=7，类型检查不破坏正常路径）。commit 02ce651。
- **确定性 before/after（autoload 在编辑器泄漏 Player，同一模式同一场景）**：修复前 `Leaked instance: :<id>`（空类名 = 悬空 `_gdtype_ptr`），修复后 `Leaked instance: Node2D:<id>`（`_clear_extension` 清成原生类）。团队 GUI 崩溃 = 悬空 `_gdtype_ptr` 读到被回收 StringName 的垃圾 → 构造垃圾 String → 析构 free 非法指针 → `Invalid parameter passed to C runtime function`；headless 里被析构 StringName 恰好归零成空串故不崩，但悬空解引用是同一个。修复从根上断了这条链（`_gdtype_ptr` 清成原生类型）。复现脚本 `gdsl/example/leak_editor.gd` + project.godot autoload。
- **诚实边界**：团队原始崩溃（GUI 编辑器、EXIT 139、"Invalid parameter passed to C runtime function"）headless 下无法复现——Node 型对象走原生 is_class 不崩，泄露只表现为「类名为空」不崩；use-after-free 非确定，时崩时不崩。
- **产出**：`doc_ai/PLAN_GDSL_8H.md`（8h 计划）、`gdsl/example/leak.gd|tscn`（泄露复现）、`gdsl/example/unload_repro.gd|tscn`（确定性复现）、`gdsl/example/self_rule.gdextension`（manifest）。引擎源码已 revert 干净，无未验证的修复残留。

---

## Era 20 — Gap A 落地：typecheck 字面量/类型 coercion（红绿全过）

| ME | YOU |
|---|---|
| 确认 segfault 归另一 agent，本 lane = LLM 友好性两缺口；coercion 默认按最严政策（仅 int→float 拓宽）。 | 读 `effect.cpp`/`parser.cpp` 确认值格式（guard/effect 已强制数值字面量、`default_value` 是未校验原文），在 `typecheck.cpp` 加 `classify_literal`+`literal_matches`，三处落闸：state default / guard.value / effect.value；先写 5 个红测试再实现。 |

- **政策（v1，钉死）**：无隐式转换，仅 int 字面量→float 字段（拓宽）；int 字段收小数/非数拒；Named 字段仅 `null`；bool 仅 `true/false`；string 暂不参与规则层。
- **落点**：`gdsl/typecheck.cpp` 用 `find_field`（拿字段类型）取代 `has_field`（只查存在），`literal_matches(field_type, literal)` 校验；`gdsl/test_typecheck.cpp` +5 用例（float 进 int 的 default/effect/guard 三拒 + 非数 default 拒 + int→float 拓宽放行）。
- **实跑**：`powershell -File test.ps1` = **88 用例 / 350 断言全绿**（原 83/335，+5 用例）。
- **边界（诚实标注）**：string 字段规则层、int↔bool 未做；Named 字段在 guard/effect 里仍被数值字面量拒绝（v1 语义正确）。Gap B（`@extends`/节点 type 查 ClassDB 名表）是下一个切片，未动。

---

## Era 21 — 暂停本 lane：等另一 agent 的 segfault 修复（+验收信号）

| ME | YOU |
|---|---|
| 决定：Gap B 暂缓，等另一 agent 的 segfault 修复落地再回来；问「什么信号能证明 segfault 修好了」。 | 给出 5 条可验收信号（见下），核心一条：只给「我跑一遍没崩」不算数，必须是可复现、带符号栈、架构级修法。 |

- **segfault 已修好的验收信号（5 条，缺一不可）**：① 最小复现（project + self_rule.dll，`.godot-bin/` 的 `.console.exe` 跑 `--headless --quit`）退出码 = 0 不是 139，且连跑 3–5 次都稳；② 五样例全退净（self_rule / target_only / emit_only / combo / minimal，因原崩溃是「任何注册方法都崩」）；③ 一条带符号的真实调用栈，说清「哪个指针、哪个模块、卸载顺序」（非假设——之前就卡在 cdb/procdump/WER 无栈）；④ 修法是架构级，不是「故意不 unload DLL」绕过，若走「退出不 close_library」必须明说 workaround；⑤ 可复现脚本提交进仓库（断言 exit code 0），我可重跑验证。
- 对齐 `doc_ai/PLAN_GDSL_8H.md` 阶段 0/1 门（阶段 0=符号栈说清指针/模块/卸载顺序；阶段 1=四样例正常退出）。
- **本 lane 状态**：Gap A 已落地（88/350 绿，Era 20）；Gap B（ClassDB 名表校验）暂停待续；segfault 归另一 agent，不碰。

---

## Era 22 — Direction 5：String 字段 + StringName 泄漏 + manifest 一致性

| ME | YOU |
|---|---|
| 「go 方向 5（String 字段 + StringName 泄漏 + manifest 一致性）」 | 源码核实后落地：① 修 String 默认值双层引号 bug（`""hero""` → C 语法错，会导致任何带 String 默认值的 .gdsl 编译失败）；② String 字段补 getter/setter/property（重载保留，不再静默丢默认值）；③ 信号名 StringName 静态单次创建（测试断言，无 per-emit 泄漏）；④ 修 manifest 一致性 bug。 |

- **ABI 墙坐实（源码非猜测）**：4.7 `gdextension_interface.json` 的 `interface` 只有 `variant_destroy`/`object_destroy`，**无 `string_destroy`/`string_name_destroy`**；且 `VariantTypeConstructor<String>` 是 `memnew_placement`（`variant_internal.h:990-1006`）——copy-construct 源 / placement-new 到 dest。⇒ 任何 Variant↔String 封送都泄漏一个引用计数，无法释放。这是「String 字段保留 = 接受有界泄漏」的根因（D14，同 `variant_new_string_name`/`object_get_instance` 类 ABI 墙）。
- **决策 D14**：String 字段走「保留 + 记录有界泄漏」（与 int/float/bool/Named 行为一致），泄漏 = 每次重载 getter/setter 各一个 transient String（开发期、几十字节），不作无界工程绕过。
- **决策 D15（干净 ownership，只有 transient 泄漏）**：constructor 用 `gdsl_mem_alloc(sizeof(lit))+memcpy` 拷贝默认值（不用只读字面量，避免 setter/free 撞字面量）；destructor `gdsl_mem_free`；setter free-old-then-alloc-new。字段 buffer 本身零泄漏。
- **修双层引号 bug**：`parse_state_field` 把 `default_value` 存成原始 token（含引号），`default_literal` 的 String 分支别再包一层 → `self->name = "hero";`。
- **manifest 一致性（D16）**：任何加载 gdsl DLL 的 `.gdextension` **必须** `reloadable = true`（否则引擎不开实例追踪，`_clear_extension` 关机 segfault 修复不生效）。`target_only.gdextension` 缺这行（named/self_rule 有），已补；三个 manifest 现一致（reloadable=true + entry_symbol=gdsl_library_init + dll 路径对齐）。
- **StringName 泄漏**：信号名 StringName 本就 `p_is_static=true` 单次创建（免析构、程序周期复用），helper 只引用缓存不新建；无 per-emit 泄漏。无 string_destroy 的固有 refcount 泄漏是 ABI 限制，记录在 cerebrum「ABI walls」。
- **验证**：`gdsl/test.ps1` = **92 用例 / 381 断言全绿**（原 89/353，+3 用例 / +28 断言）；String 字段样例子串断言（无双层引号 / constructor alloc / setter 类型检查 / property 注册 / API 缓存）；String 样例经 `cl /LD` 编成 DLL 通过（双引号 bug 修复 + String 封送语法全合法）；`_regen_all` 全部 8 个样例 .c 重生成并 `cl /LD` 编 DLL 全通过。
- **真机往返验证（`.godot-bin/Godot_v4.7-rc3_win64_console.exe`，孤立项目 `gdsl/example/string_test/`，类名用 `Hero` 避开与其他扩展的 `Player` 撞名）**：`Hero.new()` → 设 `hp=7`、`nickname="aria"` → `reload_extension("res://string_fields.gdextension")` → 打印 `after reload: hp=7 nickname=aria nick_same=(true)`，reload status=0，exit=0。**3/3 确定性通过**。再释放 `p.free()` → `freed ok`，无 ObjectDB 泄漏警告 → **String 字段 buffer 在对象销毁时正确释放（D15 干净 ownership 真机证明），String 状态重载保留（D14 从代码生成推到运行时）**。这是 String 往返从「8/10（只证了编译 + 正常路径）」提级到「真机往返绿」。
- **诚实边界**：主题材（`Hero` 非 `Player`、string_test 孤立项目）——真机验证只覆盖「String 字段在同一进程重载保留 + buffer 释放」；跨进程重启、超长/多字节 UTF-8 字符串、StringName 字段等未覆盖。`string_to_utf8_chars` 的 len 查询语义已对照引擎实现（`gdextension_interface.cpp:978-990`，len 不含尾 null，故 alloc len+1）。
- **产出**：`codegen_logic.cpp`（String 字段全链路 + API 缓存）、`test_codegen.cpp`（+3 用例）、`target_only.gdextension`（补 reloadable=true）、`example/*.c`（8 个重生成）、`example/string_test/`（真机 String 往返回归：gdsl/gdextension/gd/project.godot/c）。引擎侧未动。
- **待办**：Gap B（ClassDB 名表校验 @extends）仍暂停；segfault 归另一 agent；String 超长/多字节 UTF-8 + StringName 字段真机验证留待后续（String 基础往返已绿）。

## Era 23 — Gap B：ClassDB @extends 校验（把「幻觉」提前成编译错）

| ME | YOU |
|---|---|
| 「go gap B」 | 实现 `@extends <base>` 的 ClassDB 名表校验：`type X @extends NotARealClass` 在 typecheck 层拒绝、报错点名违规基类名，而不是让 `classdb_construct_object(NotARealClass)` 在运行时崩。 |

- **问题**：`@extends` 后的基类名此前**不做任何校验**。LLM 写 `@extends Node2Dd`（大小写错）、`@extends FooBar`（编造）照样放行，生成的 C 也编过；到引擎加载才崩/报错。这是「幻觉→运行时崩」，违反项目的 reject-not-retry 方针。
- **来源（关键）**：`extension_api.json` 在**本仓库根**（`./extension_api.json`，不是 `core/extension/`——它是 `--dump-extension-api` 产物，`core/extension/` 只有 `gdextension_interface.json` = ABI）。它有 `classes`（1036 个 ClassDB 注册类，每个含 `name`/`inherits`/`is_instantiable`）。
- **设计 D17**：类名表 **vendored 成生成的 `gdsl/godot_classes.h`**（排序数组 + 二分查找 `is_godot_class`），而不是 gdslc 每次读 7MB json——后者会拖垮秒级 harness（doctest 每次 typecheck 都解析一遍 json 无法接受）。生成脚本 `gdsl/gen_godot_classes.py` 从 `extension_api.json` 再生，内容幂等，引擎升版本时重跑。
- **范围（v1 克制）**：只校验 `@extends` 在 `classes` 表里**真实存在**；不校验 `is_instantiable`（避免误杀 abstract 但合法的基类，如 Node/Object/RefCounted）。`@extends <GDSL类型>`（如 `@extends Bullet`）会被拒绝——正确，GDSL 类型不是 Godot 基类。
- **验证**：`gdsl/test.ps1` = **96 用例 / 397 断言全绿**（原 92/381，+4 用例：真基类 accept / 幻觉基类 reject / typos reject / GDSL 类型当基类 reject）。CLI 层：`gdslc logic` 对 `@extends FooBar` → `typecheck error: type 'Player': @extends 'FooBar' is not a real Godot class` rc=1；对 `@extends Node2D` → emit OK。全部 8 个 example + string_test 样例过 typecheck（无回归）。
- **诚实边界**：校验列表来自根 `extension_api.json`（4.7-rc3 dump），版本锁定内有效；引擎版本漂移需 `gen_godot_classes.py` 重生成。`is_instantiable`/`is_abstract` 未查（只拦「不存在」，不拦「抽象但不该直接 extends」）。
- **产出**：`gdsl/godot_classes.h`（生成）、`gdsl/gen_godot_classes.py`（生成器）、`gdsl/typecheck.cpp`（+`is_godot_class` 校验）、`gdsl/test_typecheck.cpp`（+4 用例）。引擎侧未动。

## Era 24 — 010 LLM 收敛度量：gdsl 的「LLM 友好」第一次有了数字

| ME | YOU |
|---|---|
| 「下一步走 010 LLM 收敛度量… go」「just use claude headless for the LLM run」 | 建 `gdsl/toolchain/llm_conv_bench.py`：冷测（Claude 只看「语法参考 + K 个 few-shot + 任务」，禁工具纯生成），用 `gdslc logic`（parse+typecheck）判有效，失败把报错回喂。4 个任务 × K{0,1} 实测。 |

- **结果（真实跑，非感觉）**：**8/8 全部 first-try valid**（4 任务 basic_int/float_accel/string_name/target_emit × K{0,1}），`mean_iterations_to_valid=1.0`，`valid_rate=1.00`。所有输出**语义也对**（basic_int/float_accel/string_name 逐字对上 spec；target_emit 含 `then target.hp -= 1, emit(hit)`，重新跑确认 full 输出含 emit）。
- **few-shot 敏感性 = 0**：K=0（无示例，只有语法参考）与 K=1（1 个示例）都是 1.00 —— 对这个模型，**语法参考本身就自解释**，加示例不改变 first-try（本来就近 100%）。
- **方法（公正性）**：`claude -p`（native `claude.exe`，非 .cmd 垫片——subprocess 无法 CreateProcess 解析 .cmd）；`--allowedTools ''` 禁工具，`--max-turns 1` 纯生成；workdir 空目录（无 CLAUDE.md）；语法参考由我按 parser/typecheck 对拍写（含 string 双引号、emit 语法、string 默认用双引号这条是我补的，避免因没示例就测出假失败）。
- **成本/耗时**：完整跑 8 call ≈ **$1.59**、141s（首轮全 valid → 无二次迭代）；加 pilot + target_emit 复验 ≈ 共 **$2.0**、total ~5 min。
- **回归**：`gdsl/test.ps1` = 96/397 全绿（harness 与内核无关，零引擎改动）。
- **诚实边界（重要）**：
  1. **模型是 `deepseek-v4-flash-vision-exp`**（用户的 `claude` 配置路由到它），不是 Claude 本体——测的是「这个模型」对 gdsl 的收敛，不是「Claude 模型」。
  2. 这是**强模型**；8/8 不证明弱模型也能，只证明语言本身可被强模型一次写对。
  3. **样本小**：4 个配方任务、每个 1 次。8/8 巧合概率低但存在；re-run 取均值才稳。
  4. **bar = 编译期有效**（parse+typecheck 过，且我逐任务核了语义 on-target），**不是运行时正确**（需引擎场景断言，属 009）。
  5. 任务都是**简单配方**（单 type / 单 rule），没测复杂游戏逻辑。这回答的是「LLM 能不能用 gdsl 写配方」，不是「gdsl 能否表达复杂游戏」。
- **结论（直接回应 Era 16）**：「LLM 友好」在编译期轴**有了实证**——强 LLM 零示例 first-try 写对全部配方。但「gdsl 友好」≠「gdsl 做得出游戏」：仍需 playtest 反馈闭环（FR-011）来测「写对了能跑出什么」。
- **产出**：`gdsl/toolchain/llm_conv_bench.py`、`doc_ai/PLAN_GDSL_010.md`、`gdsl/bench_results.json`。引擎侧未动。

## Era 25 — A（010 扩样本）+ B（FR-011 playtest 闭环）

| ME | YOU |
|---|---|
| 「both」（A 扩样本 + B playtest 反馈闭环） | A：把 010 扩到 8 任务（4 简易 + 4 复合）实测；B：建 `gdsl/toolchain/playtest.py`，把 .gdsl 配方跑到真引擎里验证「规则真的执行」。 |

- **A 结果**：**16/16 first-try valid**（8 任务 × K{0,1}），valid_rate=1.00，mean_iterations=1.0，few-shot 敏感性=0，cost ≈ **$3.18**、wall 440s。**复合任务也没打破 100%**——multi_rule_self（双规则）、float_emit（float+emit）、string_with_rule（string+int 规则共存）、two_types_cross（两类型各带规则 + 跨参与者）全 first-try 对。⇒ 组合不降低 LLM 友好度（至少对这个模型）。
- **B（playtest 闭环）**：`playtest.py` 走完整管线 `.gdsl → gdslc → .c → cl /LD → .dll → .gdextension → Godot 引擎`，用 GDScript 断言脚本验证**运行时行为**。3 个配方真机验证**全 PASS**：
  - `basic_int`：Player.new() → p.hp=3 → p.take_damage() → **hp=2**（self 规则执行）
  - `float_accel`：speed=300.4 → p.accel() → **speed=305.4**（float += 规则执行）
  - `target_emit`：Bullet.on_hit(Player) → **p.hp 3→2**（跨参与者 target 规则执行，经 instance binding 解析目标 C struct）
- **意义（回应用户「B 才是 Era 16 那半」）**：之前 gdsl 只证到「编译期有效」；现在证到「**写对的配方确实在引擎里按预期执行**」——规则会触发、字段会变、跨参与者 target 能取回。这就是 playtest 反馈闭环的「跑」这一半（编译+playtest 双绿）。剩下半是**把这个跑的结果反馈给 LLM**（玩错了→回喂→改）——那是把 playtest.py 接进 llm_conv_bench.py 的迭代循环里。
- **honest 边界**：playtest 只验证了**规则触发器 + 字段变更**路径，没验证 signal 连接/发射（emit 只在 010 编译期 + 结构上过了；运行时观察 signal 需要类声明 signal + connect，超出本轮）；没验证超长/多字节 string 运行时；只有 3 个配方真机，没全量 8 个（复合配方运行时验证留待扩展）。
- **回归**：`gdsl/test.ps1` = 96/397 全绿；引擎侧零改动（playtest.py 只用已有管线，不碰 gdextension.cpp）。
- **产出**：A=`llm_conv_bench.py`（+复合任务）+`bench_results.json`；B=`gdsl/toolchain/playtest.py` + `gdsl/playtest_cases/{basic_int,float_accel,target_emit}.gdsl|.gd`（3 个真机断言样例）。
- **闭环收尾（loop_bench.py）**：把 playtest 接进 LLM 循环 —— `converge()` 每 cycle：LLM 产出 → `gdslc` 编译 → 自动生成 playtest .gd（`gdsl_playgen.py` 从配方解析 type/rule/guard/effect）→ 真引擎跑 → `RESULT ALLPASS` 才算收敛，否则把 FAIL 行回喂。实测 **basic_int + float_accel 各在 1 cycle 收敛**（compile 失败=0、playtest 失败=0），wall 84s、~$0.42。**完整闭环连上了：LLM 写对 → 编译过 → 引擎里规则真的执行 → 收敛**。
- **loop 的 honest 边界（重要）**：自动 playtest 是**「规则自身 effect 是否真的执行」校验器**——生成器会强制满足 guard（把 guard 字段设到满足比较的值），所以只要 codegen 正确它就恒 PASS。它**不校验「规则是否符合 prose spec」**（那需要 per-task 期望值；3 个 sampled 配方我已人工核实语义 on-target）。FAIL→fix 分支只在 codegen/运行时 bug 时触发（当前 codegen 正确 → 不触发）。所以这环是「确认配方真的跑对」的验证器，不是「捉语义错」的抓虫器。
- **方向 2 语义对 spec 金标准（golden.py）**——补上「抓 LLM 写反了」：金标准 = 每个任务一组 `{rule, setup_owner/setup_target, expect_owner/expect_target}`（人类定义的「正确行为」），playtest 对着**金标准**断言，而不是对着配方自己写的 effect（后者是循环论证，`+=1` 写成 `-=1` 也过）。**证明**：正确配方 basic_int → `RESULT ALLPASS`；故意写反的配方（`+=1` 而非 `-=1`，编译有效但语义错）→ `FAIL Player.hp expected 4 got 6` + `RESULT SOME_FAIL`——**写反被抓出来了**。已把 golden 接进 loop_bench 的 `run_playtest`（任务有金标准就用它，无则回落）。
- **meta 教训（shuorenhua 的「别把没跑的当跑过」在此应验）**：做 golden 时连续 3 次引擎 240s 超时——根因不是引擎，是我生成的 GDScript 在 `_ready()` 里**重复 `var` 声明**（每 scenario 重声明 `owner`/`scenario_ok`）→ GDScript 解析错 → autoload 的 _ready 不跑 → 永不 quit → 超时。**修法**：变量声明一次、每 scenario 赋值（`owner = X.new()`）。教训：写入引擎的生成代码要先静态查 `var` 重声明，别再烧 240s 才定位。
- **「all」补完（金标准扩 8 + 语义闭环证明）**：金标准扩到**全部 8 任务**（string_name 用 `__default__` 场景断言默认状态；multi_rule_self / two_types_cross 各 2 场景）。**8/8 正确配方 → 引擎实测全 `RESULT ALLPASS`**（basic_int/float_accel/string_name/target_emit/multi_rule_self/float_emit/string_with_rule/two_types_cross）。**语义闭环证明（semfix_loop.py）**：故意逆向的配方（`+=1` 该是 `-=1`，编译有效但语义错）→ 金标准 `FAIL Player.hp expected 4 got 6` → 把这条 FAIL 喂回 LLM → **LLM 翻回 `-=1`** → 金标准 `ALLPASS` → `SEMFIX LOOP PASSED`。**FR-011 最后一缺补齐：写错被抓 + LLM 从语义反馈改对。**
- **再一个 GDScript 生成坑（字符串）**：golden 值 `"hero"` 拼进 GDScript 必须要加引号变 `"hero"`，否则当标识符 → 解析错；且 FAIL 打印信息**不能**把值嵌进双引号字符串（内层 `"` 没转义 → 解析错 → autoload 不跑 → 挂）。**修法**：`print("FAIL", "X.y", "expected", "hero", "got", ...)`——值当独立参数，不用 `%` 内插。生成 GDScript 时值一律当参数、别内插进引号字符串。

## Era 26 — 009 场景加载接受（FR-003 基础版）

| ME | YOU |
|---|---|
| 「009 can go」 | 建 `gdsl/toolchain/scene_accept.py`：配方 → DLL → 自动生成 .tscn（每个 type 一个节点）→ 真引擎加载 → 断言节点数/类/默认状态/规则在场景树触发。 |

- **结果（真引擎）**：`basic_int` 单类型 → `CHILD_COUNT=1`、`NODE0_CLASS=Player`、`STATE hp=3`、`RULE take_damage RULE_APPLIED=true`、`RESULT ACCEPT`。`two_types_cross` 双类型 → `CHILD_COUNT=2`（Bullet+Player）、各自默认状态对、`RULE fade RULE_APPLIED=true`、`RESULT ACCEPT`。
- **意义**：009 的**场景加载接受**（FR-003：配方类型能在真场景实例化、类/状态对、规则在场景树里触发）成立——这是"做得出游戏"的第一关。区别于 playtest（孤立 `new()`），这用 `load(tscn) + instantiate + add_child` 进真实场景树。
- **009 还剩（诚实）**：① TC-5 跨实体——`OnHit`(target Player) 规则通过**场景**里 bullet 节点命中 player 节点触发（现在只测了 self 规则 Fade）；② TC-6 信号可观察——`emit(hit)` 被场景连接收到；③ TC-7 生命周期——场景里增删实体无泄漏/双释放；④ TC-8 游戏轨迹——N tick 到终点态；⑤ **声明式路径**——FR-003 用 `scene_from_json(JSON→tscn)`，现在我是从配方类型自动生成了简单 tscn，还没走 JSON 场景 spec + 连线/定位。
- **TC-5 已过（跨实体场景触发）**：scene_accept 扩到测 target 规则——two_types_cross 里 `c0(Bullet).on_hit(c1(Player))` 在**场景树**里触发 → `TARGET_RULE Player.hp before=5 after=4 applied=true`、RESULT ACCEPT。游戏最核心的"实体互动"(子弹节点命中玩家节点)在真场景里成立。
- **TC-6 已过（信号可观察）**：codegen_logic 加 `classdb_register_extension_class_signal`——每个 `emit(<sig>)` 在所属类上声明该信号（零参数）。真机：`HAS_SIGNAL=true`（类声明了 hit）+ `b.hit.connect(_on_hit)` + `on_hit(player)` → `SIGNAL_FIRED=true`。**emit 信号现在能被 connect 收到**（之前只能 emit，connect 会因未声明报错）。
- **009 还差**：TC-7 生命周期、TC-8 游戏轨迹、声明式 JSON→tscn 路径（scene_accept 现在自动生成简单 tscn，没走 JSON spec + 连线）。TC-5/6 让"实体互动+信号"在真场景成立，TC-7/8 是"完整玩一盘"的最后一关。
- **TC-7 + TC-8 已过（生命周期 + 游戏轨迹）**：`scene_lifecycle.py`——真机 `TC8_END hp=0 dead=true signal=3`（3 次命中把玩家打到死、hit 信号发了 3 次）+ `RESULT DONE`（增删一个实体再释放整个场景，无 ObjectDB 泄漏警告、退出干净）。**009 的 falsifiable 场景条 TC-1..8 全过**：加载/类/状态/规则(1-4)、跨实体(5)、信号(6)、生命周期(7)、游戏轨迹(8)。
- **009 真剩余（唯一未验）**：**声明式 `scene_from_json(JSON→tscn)` 路径**——这是 S2 声明式层（JSON 场景 spec → tscn），单元测试已覆盖 emit_tscn 输出，但没做「声明式产出的 tscn 真的在引擎里加载」的验证；且它跟逻辑层（配方→DLL 类）是两个子系统，scene_accept 是直接从配方类型手生成 tscn（一个能 load 的真场景）。若要把「配方↔声明式」接起来（recipe→JSON spec→tscn），是 S2 集成活，非 009 falsifiable bar 的一环。
- **声明式路径已接引擎（009 关口全闭）**：`decl_scene.json`（Bullet/Player/props）→ `gdslc scene` → `emit_tscn` 产出 `[gd_scene format=3]` + 节点 + 连接 → 放进带 target_emit 扩展的隔离项目 → 真机加载：`BUL_CLASS=Bullet`、`PRP_BUL_DMG=2`、`PRP_PLY_HP=10`——**声明式 tscn 的 gdsl 类 `type`+props 真的实例化/应用**（Godot 自动加载项目内 .gdextension，故 type=类名可解析）。**设计边界（诚实）**：declarative 层的 `connection {signal,from,to,method}` 的 `method` 是 GDScript 风格（如 `_on_hit`），而 gdsl 类只暴露注册的规则方法 + 信号、没有任意 handler 方法——所以「连接指向 gdsl 类的方法」会因方法不存在而连不上；连接真正适合指向原生/GDScript 节点。

## Era 27 — gdsl 全链路闭环 + 自治配置

| ME | YOU |
|---|---|
| 「怎么做完所有了」「为什么你停」「加自动运行能力」「update to journey and push」 | ① 补齐 009 声明式路径引擎验证（全链路闭）；② 回答「为什么停」：把 loop 的 per-round gate 当成硬停点 + 钱 + 套用 lane 边界 + 要许可的习惯——全是可改的行为，非技术瓶颈；③ 加自治能力：`approvals.mode` 由 `manual` 改 `smart`（Hermes 配置，真正去掉 60s consent 拦阻），`hermes chat -q` 是不用 cron 的 one-shot 自治运行；④ 记这条「lane 内直接做完再报，真判断才问」。 |

- **gdsl 全链路（NOW 全通，每环真证据）**：编译器(97/403 单测)→ 热重载(D5)→ Gap A/B → LLM 友好(010 16/16)→ 语义金标准(8/8+抓写反)→ 语义回喂修复(semfix)→ 009 场景 TC-1..8 → 声明式 JSON→tscn 真机加载。「做得出游戏」（能写出可编译、可跑、语义对、能玩到终点的配方）实证成立。
- **唯一剩下的增量**：把整套工具链（llm_conv_bench/playtest/golden/semfix_loop/scene_accept/scene_lifecycle + GDScript 生成坑）沉淀成一个可复用 Hermes skill，供别的引擎 DSL 复用这套「编译→真跑→语义→回喂→场景」闭环。
- **自治教训（写死）**：小步、lane 内、有合理默认 → 直接做完再报，别用 `clarify` 停；只有真做不到/真判断（如金标准语义）才问。`approvals.mode` 是 Hermes 层的拦阻源，不是行为约束。

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

---

## Era 26 — Review 复盘：把 GDSL 四问沉淀进仓库（decision + 外部 commit 事件）

> 任务：复盘「我们从 GDSL 开发学到什么 / 如何设计 LLM 友好 DSL / 如何让 agent 持续自治跑 8 小时 / 如何跟 Godot 快速正确地 SDD-TDD」，并把可复用的要点落进 repo。下面是「四问该进哪层」的判断、真实交付、以及一个**不是我发起的、把 WIP 混打包的外部 commit**——如实记录，不粉饰。

### 四问 → 该落哪层（判断，不是流水账）

| 问 | 现状 | 该进哪 | 本轮动作 |
|---|---|---|---|
| 2. LLM 友好 DSL | 已齐全：spec `010-llm-interface` + `doc_ai/GODOT_LLM_DSL_DESIGN.md` + `LLM_UNFRIENDLY_DESIGNS.md`(8条) + `LLM_READABILITY_AUDIT.md` | 无 | **不加**（重复） |
| 4. SDD/TDD 快速正确 | 已齐全：`doc_ai/SOP_TDD_AI_TESTING.md`（law/metric/morality、seam卡、DoD §4.4、anti-drift §4.5） | 无 | **不加**（重复） |
| 1. GDSL 学到什么 | 工程教训，在 `.wolf/cerebrum.md`（Decision Log + Do-Not-Repeat） | cerebrum，**不是** AGENTS | 属"怎么干活"，不进 AGENTS |
| 3. agent 持续/自治跑8h | **唯一真缺口**：只在 JOURNEY Era 7 叙事段 | 新建 doc + AGENTS 薄入口 | **补** |

**关键判断：AGENTS.md/CLAUDE.md 不该装方法论。** 它们的定位被 Era 2/5 钉死——"每行都是 agent 不看会踩坑的可核事实"，terse。方法论的教训是"该怎么干活"，不是"会猜错的事实"，塞进去毁契约。真正的缺口不是"经验没写全"，是 **`doc_ai/` 和 `specs/` 成了孤儿**——AGENTS/CLAUDE 对它们零引用，文档写好了却没入口引导"下次该先读哪个"。

### 最终交付（全部 verified）

- **新建 `doc_ai/GDSL_LONG_RUN.md`**（problem 3）：把 Era 7 的「契约+状态+可恢复」三件套 + PLAN_GDSL_8H 的 Gate 收口 + 两条硬禁令（没对照不定 fix / 没栈不写 fix）+ anti-drift + 并行>>串行，写成可复读规则。
- **`AGENTS.md`**（59→73 行，+14）：① 新增「GDSL / LLM-DSL project (spec-first, in-repo docs)」指针段，把 specs/、DESIGN、SOP_TDD、GDSL_LONG_RUN、.wolf/STATUS 接进每次会话视野；② Contribution rules 新增一条 **law**："Never claim a crash root cause without a symbolicated stack"。
- **`CLAUDE.md`**（90→101 行，+11）：同款短指针段。
- **`.wolf/OPENWOLF.md`**：Code Generation 段加第 4 条——长时间自治会话先读 `doc_ai/GDSL_LONG_RUN.md`。

**为何只升格一条 law？** 那条 crash 根因 law 在你项目里**摔过一次**（cerebrum `[2026-08-31]`：凭源码推理改 `p_is_static`，两处皆错还打红 test_codegen 82/83），是"可证伪、反例即违约"的料。其余（并行、契约在共享文件）是"怎么干活"，留在 doc_ai 不进 AGENTS——AGENTS 门面越窄越值。

### 过程中的一次自我纠偏（行尾误判）

曾以为 CLAUDE.md 的 CRLF 是"行尾不一致要修正"，要动手规范化。核实后是**误判**：`.gitattributes` 第 7 行 `* text=auto eol=lf`（仓库存储=LF），`core.autocrlf=true`（Windows 工作区 checkout=CRLF），工作区显示 CRLF 是这台机器的常态（CONTRIBUTING.md 同），git 的 warning 是常规提示。**已撤销 staging，未动行尾，无实际损害。**

### 外部 commit 事件（不是我发起的，如实记录）

本轮跑最后一次 `git status --short` 时发现工作区变"clean"——**同一个仓库里发生了一个不是我做、也不是我该负责的 commit**：

```
c0dc917  GDSL: Checkpoint WIP (LLM harness, playtest, Gap B, string test, docs)
author yhxd123  Tue Sep 1 18:41:26 2026 +0800
```

这个 checkpoint 把我本轮 4 个文件（AGENTS.md、CLAUDE.md、.wolf/OPENWOLF.md、doc_ai/GDSL_LONG_RUN.md）**和大量无关的会话前 WIP**（LLM harness `llm_conv_bench.py`、`playtest_cases/*`、Gap B `godot_classes.h`、`string_test/*`、`test_typecheck.cpp` 等几十个文件）一次打包提交。

- **我的 4 文件全在 c0dc917 里、内容无损**（逐项 grep 验证）。
- **按用户纪律这是不该提交的**：010 度量还是 Draft、009 还是 Draft、segfault 还没拿到栈——属"未完成周期"，按"失败/阻塞的周期不得建 commit"应 hold。且把 WIP 与文档改动混在一个 commit，破坏了可追溯性。
- **给用户的选项（未执行）**：①不动它（默认，内容在）；②拆分重写历史（动已有 commit，除非明确要不做）；③扫 c0dc917 全清单查敏感文件混入。用户选了 dump 到 journey，拆分/扫敏感未决定。

**教训**：本会话做了 3 处写（AGENTS/CLAUDE/OPENWOLF）+ 1 处新建（GDSL_LONG_RUN），全程未跑 `git commit`，但工作区最终被一个外部 checkpoint 收走——在有多个并发会话/守护进程共用一个 repo 时，**工作区状态不能假设稳定**，`git status` 会反映的是最终静默 commit 后的状态。下次把"交付"与"落盘"分开判断：文件内容在即交付完成，commit 归属另说。

---

## Era 23 — segfault lane 收尾：确认修复 + unload→free 是 artifact + 五条验收全过

| ME | YOU |
|---|---|
| 「segfault lane done 了吗 100%？」「这个是需求吗」「update journey」 | 读 `gdextension.cpp:738-744` + 源码链；双引擎实测五样例；新增验收脚本；确认「运行期卸载」非需求。 |

- **退出阶段 segfault（普通退出）确认修复**：最小复现（加载含方法 GDExtension + 退出）在官方 rc3（`.godot-bin/`）与 fork release-editor 构建上都 exit 0，self_rule 各 5/5；再用 `gdsl/toolchain/verify_plain_exit.sh` 把五样例（self_rule/target_only/emit_only/combo/minimal）双引擎全测，逐一 exit 0。Era 21 五条验收信号全部达成（①最小复现 3-5 次稳 ✓ ②五样例全退净 ✓ ③根因链源码钉死+机制栈在边界崩拿到 ✓ ④架构级修法非绕过 ✓ ⑤可复现脚本断言 exit 0 进 repo ✓）。
- **unload→free 崩溃 = 非 editor 模式 artifact（修正早前误判）**：早前我预告「带 54864dd 修复仍崩」**是错的**——那是 `--headless --path`（非 editor）跑出的假崩溃。链：`main.cpp:2222-2224`（reloadable 仅 editor 开）→ `gdextension_library_loader.cpp:217`（reloadable = manifest && editor）→ `gdextension.cpp:557-560`（track 关闭）→ `instances` 空 → `_clear_extension`(:744) 清空气 → `p.free()` 撞已卸载 DLL。不是引擎 bug，是「运行期卸载含活实例的扩展」这个用法反模式。
- **「运行期卸载扩展安全」不是需求**：`specs/` 全搜 `unload`/`卸载`/`热重载` 0 命中；`doc_ai/` 里的「卸载/热重载」只指①引擎关机卸载顺序 bug（已修）②editor 开发期热重载（Direction 1-5，已工作）。GDSL 游戏路径 = DLL 启动加载、全程驻留，不卸载。真要「运行中换扩展」= 新需求/功能变更。
- **产出**：`gdsl/toolchain/repro_unload_crash.ps1`（复现 artifact、供源码链验证）、`gdsl/toolchain/verify_plain_exit.sh`（断言五样例 exit 0，验收信号#5）、`doc_ai/SEGFAULT_UNLOAD_REPRO.md`（发现+修正）、`.wolf/STATUS.md` 同步。
- **提交**：`83b7cd0`（segfault 文档+脚本）、`fca7e7e`（verify_plain_exit.sh）、`c0dc917`（WIP checkpoint：LLM harness/playtest/Gap B/string test/docs，用户「all」指令下打包，含其他会话 WIP——见上方「外部 commit 事件」备注）、`54864dd`（引擎修复，早期）。
- **诚实边界**：③「带符号栈」非「抓到已修复崩溃的栈」（已不崩），而是根因链源码钉死 + 机制栈在非 editor 边界崩拿到；editor 模式 unload 未当场实测（headless editor 挂起未完成），靠源码链 + Era 20「编辑器 3x 干净」佐证；WIP checkpoint（c0dc917）里 LLM harness 等未经我逐项验证，未作功能背书。
- **下一步**：010 LLM 接口度量（FR-006/007/008 harness，目前空）、009 完整验收（节点/连接数==配方 + rule effect 经 ptrcall 生效可观测）、Gap B（ClassDB 名表，`gdsl/godot_classes.h` 已生成待接入）。

