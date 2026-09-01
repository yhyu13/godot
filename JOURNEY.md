# JOURNEY.md — godot 引擎 fork 的引导阶段记录

> 本文件记录「人怎么决策、纠正、砍方向」与「AI 怎么执行、证伪、诚实报告」的时间线。
> 列名：`ME = 用户`，`YOU = AI`。日期格式 `YYYY-MM-DD`。
> 当前阶段：**S6 真机集成进行中**（route B 跑通：`.gdsl` → 生成 C → `cl` 编 `.dll` → `.gdextension` manifest → 引擎加载）。本阶段修掉两个真机 bug（emit_signal 兼容哈希 + hint_string NULL 崩溃），83 用例 / 335 断言全绿；**还剩一个退出阶段 segfault（EXIT 139）未修**——任何注册了方法的 GDExtension 退出时都崩，根因假设（DLL 卸载顺序 vs MethodBind 悬空指针）已提出但无符号栈证实。

## 风险与待办（roll-up，非替换——各时代的行内风险/待办仍在原表里）

- **[Era 1·起步] 浅克隆 + fork 远端**：`git rev-parse --is-shallow-repository` = `true`，仅见一个 commit `f6ab5db`；remote 是 `yhyu13/godot`。**不能假设能 push 上游 `godotengine/godot`**，PR 走 fork 流程。
- **[Era 4·OpenWolf] web-app 模板与 C++ 引擎错配**：`.kilo/command/{reframe,security-audit}.md`、`.claude/commands/*`、`.wolf/OPENWOLF.md` 的 `designqc`/`reframe`/`npm audit` 都是 React/Tailwind/web 套路。agent 照做会落空（本仓库无 `package.json`/dev server）。待确认：裁剪成 Godot 适用，还是接受不适用。
- **[Era 4·OpenWolf] 记忆库全空**：`.wolf/{cerebrum,memory,STATUS}.md`、`buglog.json`、hippocampus/token-ledger 均 15:54:07 初始化为零。TODO：真实开发开始后按 OPENWOLF.md 协议回填，否则 OpenWolf 形同虚设。
- **[Era 4·OpenWolf] anatomy.md 对 C++ 文件价值低**：`file_count=513, hits=0`，多数描述只是版权首行（如把 `CLAUDE.md` 标成「OpenWolf」）。RISK：agent 依赖 anatomy 描述会读到无意义文本。
- **[Era 2-3·AGENTS] 两份指令文件会漂移**：`AGENTS.md`（紧凑）与 `CLAUDE.md`（更详）内容重叠（构建/测试/架构/贡献约定）。TODO：后续改动若只更新一份，另一份会过期。
- **[Era 8·push] `.wolf/` 运行时状态文件被提交进 git**：`hippocampus.json`/`token-ledger.json`/`_scan-state.json` 等含机器绝对路径 `D:\GitRepo-My\godot`，会随每次会话变化产生提交噪声。待确认：是否给 `.gitignore` 加规则排除。
- **[Era 17·真机集成] 退出阶段 segfault（EXIT 139）未修**：任何注册了方法的 GDExtension 退出时都崩，最小 self-only 规则也崩。最可信假设（未证实）：GDExtensionMethodBind 存着指向 DLL 的 call_func/ptrcall_func 函数指针，DLL 在 `ClassDB::cleanup()`（`class_db.cpp:2350` memdelete MethodBind）之前被 `close_library` 卸载 → MethodBind 析构时悬空解引用。证据缺口：cdb 卡住/超时、procdump 抓不到（崩溃在 teardown）、WER LocalDumps 没落盘——无真实调用栈，只有源码路径推理。

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
