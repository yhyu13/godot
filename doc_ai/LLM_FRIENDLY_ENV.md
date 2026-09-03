# 大仓库游戏 LLM 友好开发环境（doc_ai）

> 一句话：把 LLM 不可靠的地方（命名、字符串寻址、动态类型、哈希、控制流）用编译器/校验器挡在 LLM 之外，只给它「小语法 + 强类型 + 固定符号表 + 秒级反馈」，再用契约和 Gate 让并行 agent 不漂、不编造、不越级。

**结论：能做。这套环境的骨架四条——(1) LLM 只见文本+强类型+固定 ontology，native/字符串/哈希全由编译器吃掉；(2) 并行 agent 用「文件所有权分区 + 契约/状态/可恢复三件套」，不用 git worktree；(3) 反馈环分层：秒级内环（独立测试树）+ 分钟级外环（引擎构建），别让大构建卡住小迭代；(4) 准确度靠 law/metric/morality 三层 + 「写前 grep、不编造符号」写进契约。代价：它不是通用语言，覆盖不了编辑器插件、任意算法这些长尾。**（锚点均见文末「数据出处」，逐条可 grep 回源码。）

## 是什么 / 不是什么

- 是「给 LLM 设计一个可控、可验证、反馈快的开发环境」；不是「让 LLM 更聪明地读 1M 行 C++ 源码」——后者基本做不到，前者才可落地。
- 是「让 LLM 写配方（数据+规则）」；不是「让 LLM 写程序（命令式控制流）」——命令式是它的高错率区。
- 是「把容易错的底层交给编译器」；不是「加更多工具让 agent 慢慢试错」——试错慢、不可复用。

（以上是对比句的全部，全文 ≤3 处。）

---

## 1. 设计总纲：表面对 LLM 最小化，可验证性最大化

核心一句话：**可验证 > 可表达。**（`GODOT_LLM_DSL_DESIGN.md` 的 thesis。）

LLM 适合输出「受 schema 约束、确定性格式化、每个符号都能 grep 回真实 API」的东西；不适合输出「动态类型、字符串寻址、隐式转换、闭包/递归」的东西。环境设计的全部动作就一条：把前者交给 LLM，把后者交给编译器。

两层架构落地（`GODOT_LLM_DSL_DESIGN.md` §2）：

- **声明式层**：输入 JSON + JSON Schema，编译产出 `.tscn`/`.tres` 文本，交给引擎现成的 `ResourceFormatLoaderText`（`resource_format_text.h:148`）→ `PackedScene::instantiate`（`packed_scene.h:273`）。LLM 从不手写 `.tscn`——那是 Godot 自定义文本格式（`Vector2(1,2)` 字面量、路径引用全靠约定，无正式规范，issue #7102），JSON Schema 校验不了它。
- **逻辑层**：小语法 + 固定 effect ontology，编译成 GDExtension 原生代码，走 `object_method_bind_ptrcall`（`core/extension/gdextension_interface.cpp:1350`，注册 `:1843`）→ `MethodBind::ptrcall`（`method_bind.h:119`），裸指针、零 Variant。不是跑在 Variant 字节码 VM 上的脚本。

---

## 2. 八个「不 AI native」设计对策（汇总表）

完整逐条论述在 `LLM_UNFRIENDLY_DESIGNS.md`；这里只给「坑 → 对策 → 状态」。

| # | Godot 的不 AI native 设计 | 对策 | 状态 |
|---|---|---|---|
| 1 | GDScript 默认动态类型（Variant 满天飞） | DSL 强类型字段 + 静态 typecheck，编译不过即拒 | 已落地（83 用例 / 335 断言） |
| 2 | signal/call/get_node 字符串寻址 | 引用由 typecheck 解析成类型字段；字符串→哈希由编译器算，LLM 不碰 | 已落地（哈希）/ 部分（name 校验） |
| 3 | 命令式语言（LLM 错率最高） | 声明式 when/then + 固定 effect ontology，删 while/递归/闭包 | 已落地 |
| 4 | Variant 隐式类型转换 | DSL 无 Variant，字段静态类型，转换收窄到显式 | 建议（政策未钉死） |
| 5 | `.tscn`/`.tres` 无 schema（issue #7102） | 声明式层输入 JSON Schema，`.tscn` 只当编译器输出 | 已落地 |
| 6 | ClassDB 三份 API 表示漂移 | 定单一事实源 `extension_api.json`，类型表/ontology 从它生成 | 建议（当前手写 + 手核） |
| 7 | MethodBind 按结构哈希匹配、不按名字 | 编译器从源码算哈希（`gdsl/abihash.h`，murmur3/fmix），LLM 只写名字 | 已落地（黄金值 4047867050 防漂移） |
| 8 | 资源默认二进制 + 路径/UID 引用脆弱 | 只吐文本 `.tscn/.tres`；悬空引用已校验（`gdsl/scene_json.cpp:173-193`，FR-004） | 已落地 |

四条最值钱：

- **第 7 条是 LLM 猜不中的硬事实**：MethodBind 按 `MethodInfo::get_compatibility_hash()` 结构哈希匹配（`method_bind.cpp:35-45`），跟名字无关。LLM「名字对就行」不成立——哈希必须由编译器算。
- **第 6 条是把纪律变机械**：同一方法三张脸——C++ `ClassDB::bind_method` 宏 → `extension_api.json`（6.96 MB 机器导出，1036 classes）→ `doc/classes/*.xml`（1,219 份）。从 `extension_api.json` 生成类型表/基类白名单后，「不能发明符号」从口头变可执行。当前是手写 + 手核（`gdsl/godot_classes.h` 由 `gen_godot_classes.py` 从 repo 根目录 `extension_api.json` 生成，仅 v1 校验存在性，D17）。
- **第 4 条是未钉死的真实缺口**：typecheck 可能静默放行 `hp = 3.0` 这类。政策建议「无隐式转换，仅 int→float 显式/窄化」，但没进 spec/测试。
- **第 8 条是悬空引用教训**：曾有人声称「悬空引用检查缺失」，打开源码发现 `gdsl/scene_json.cpp:173-193` 早已实现（FR-004）——**声明缺口前必须先打开源码，不从 memory/文档下结论**（cerebrum Do-Not-Repeat `[2026-09-01]`）。

---

## 3. 并行 agent 工作方式（大仓库专用）

大仓库（Godot ~1M 行 C++）不能靠「多开几个 agent 各自莽」，靠四条（`AGENTS.md`「Parallel agent work」+ `GDSL_LONG_RUN.md`）：

1. **禁止 git worktree。** 每个 worktree 要自己的完整 `scons` editor 构建（15–45 分钟 + 几 GB `bin/` + 自己的 `compile_commands.json`）。成本爆炸，从不值得。
2. **文件所有权分区。** 每个并行 agent 只碰互不相交的一组路径。分层架构 + 隔离的 `gdsl/` 树让这很容易（`gdsl/*`、`specs/*`、`doc_ai/*`、`.wolf/*` 天然几乎不撞）。
3. **从不用 `git add -A`**，只精确 stage 自己拥有的文件；提交是 orchestrator 的活儿。若某 slice 非独立提交不可，等它交接，绝不把别的 agent 已 stage 的改动扫进自己提交。
4. **引擎构建串行化，其余秒级。** 只有 slice 009 需要 `scons` 引擎构建；其他 `gdsl/` slice 跑 `test.ps1` 只要几秒。同时只允许一个引擎构建。

### 自主 agent 的「契约 + 状态 + 可恢复」三件套（缺一不可，`GDSL_LONG_RUN.md`）

- **契约放共享文件，prompt 只给最小指令（~15 行）**。纪律不塞进每次 prompt，放「可检索的一处」，prompt 只给入口。
- **状态机 + 幂等认领，不是定时器**。`[ ]/[~]/[x]` 三态 + 「先认领再动手」，7–14 个并发不撞车、崩溃能续。
- **可恢复（resume）是一等公民**。148 次派发 4 次失败（2.7%，1 prompt schema + 3 connection reset），全部 `task_id` resume 恢复。默认每次工具调用都可能是失败的。
- **并行 >> 串行**：14 批并行 64 分钟，对「每 5 分钟 1 个」的串行约 11 倍。
- **追踪器必须对账地面真值**：144 模块那次主 agent 写「145/58」，实际 144/57，靠 INDEX vs 文件系统对账抓出。不能只信自己的 tracker。
- **Gate 收口**：8 小时攻坚的骨架是「控制实验 → 抓栈 → 定 fix → 落地 → （尾巴）验收」，每阶段一条 Gate + 两条硬禁令——**没做对照实验不写 fix**、**不拿栈不写 fix**（`PLAN_GDSL_8H.md:46,52`）。

---

## 4. 反馈环分层：快编码的地基

快编码的前提是反馈快，不是模型写得快。环境把反馈环切成两圈：

- **秒级内环**：`gdsl/` 是独立 C++17 树，自带 doctest harness——`cd gdsl && powershell -File test.ps1` 几秒跑完整套。parser/typecheck/codegen/binder 的每次改动都在这里验证，**绝不触发引擎构建**（那是 15–45 分钟 + GB 级）。
- **分钟级外环**：只有 `009` 引擎集成才需要 `scons`。这个环刻意缩小到只碰真实引擎边界的那一小片。

两条放大的杠杆：

- **确定性 + canonical formatter**：编译器带确定性格式化，LLM 输出先 format 再 diff——消除空白/换行噪声，让下一次迭代只看语义差异。
- **并行吞吐**：能并行不要排队（上节 11 倍）。7×24 的价值在吞吐，不在「定时」。

性能边界（真实，不是乐观）：逻辑层走 `ptrcall` 快路径零 Variant，但官方口径瓶颈是跨语言 marshalling 不是 VM——效率优势集中在「热循环留在原生代码内、少穿边界」，不是静态类型本身。官方上限：原生类方法调用 typed vs untyped +70%~150%（GDScript typed-instructions，debug build）。

---

## 5. 准确度的三层法 + Do-Not-Repeat

准确度不靠「让模型更聪明」，靠「把错挡在门外 + 纪律穿透 agent」（`SOP_TDD_AI_TESTING.md` §0.5）。

| 层 | 中文 | 是什么 | 怎么执行 | 示例 |
|---|---|---|---|---|
| **Law** | 法律条文 | 少数、二值、机器可查、可证伪、零假阳性的门 | CI / 构建失败 | §4.4 DoD：exit 0、目标绿、无回归、红绿间测试逐字节不变 |
| **Metric** | 度量仪表盘 | 只记录趋势、**不设门** | Dashboard / 报告 | mutation score、coverage delta、cycle time |
| **Morality** | 道德约束 | 判断原则，软、允许例外 | 人 review | 改架构前先解释 |

**分层的理由（为什么混了会崩）**：

- 把偏好升成硬门 → 假红（阈值任意，CI 慢机器就红，人于是禁用法律）。
- 把硬门降成趋势数 → 失去约束（古德哈特：设成目标的度量就不再是好度量）。
- 可证伪才是法律的判据：属性/不变量（`reverse(reverse(x))==x`）是法律，一个反例即违约；行为例子（`abs(-5)==5`）是判例不是法律。

### Do-Not-Repeat（`cerebrum.md`，两条最贵的）

- `[2026-08-31]` 凭推理改 signal-name `p_is_static` true→false 追 teardown segfault，两处皆错（`string_name.cpp` 显示 StringName 总是拷贝名字，`p_static` 只影响 `static_count`），还让 `test_codegen.cpp` 82/83 红（与「全绿」报告矛盾）。**不拿栈不写 fix；crash 根因必须有符号化栈或对照实验坐实。**
- `[2026-09-01]` 声称「场景 connection 悬空引用检查缺失」，没读 `scene_json.cpp`——它已在 `:173-193`（FR-004）。**声明缺口前先打开源码。**

### ABI 墙（必须从源码 grep，不猜）

4.7 ABI 里**没有** `string_destroy` / `string_name_destroy` / `object_get_instance` / `object_emit_signal` / `variant_new_string_name`（grep `core/extension/gdextension_interface.json` `interface` → 零命中）。任何 String/StringName 值经 `string_new_with_utf8_chars`/`string_name_new_with_latin1_chars` 创建后无法释放 → refcount 泄漏（「transient String」）。跨对象取 C 结构体只有 instance-binding 路径（token-keyed）。**LLM/agent 在这类 ABI 墙上猜一次错一次，必须 grep 源码。**

---

## 6. 未验证 / 待确认（诚实标注，不粉饰）

- **逻辑层编译 vs 热重载**：GDExtension 需重编译，损失 GDScript 热重载。接受编译延迟，还是把语言做进引擎核心走路线 A——后者是核心编译级成本，不是插件能解决的。**待确认。**
- **跨语言单次调用绝对 ns 数据**：官方从未发布，所有跨语言绝对耗时**待确认**（仅有定性结论 + typed-instructions 相对加速比）。
- **effect ontology 覆盖边界**：VGDL/PuzzleScript 的效果本体硬编码、不可扩展；本 DSL 要覆盖多少玩法本体**待确认**——这是最大的设计风险，不是技术风险。
- **字面量/类型兼容政策**（第 2 节 #4）：建议「无隐式转换」，但没在 spec/测试钉死，**当前可能静默放行**。

---

## 7. 一句话总结

> 大仓库游戏的 LLM 友好环境，就是把「可生成的场景文本、LLM 可验证的小语法、反射式 C++ 绑定、可约束解码的 schema」四件事并成一份「配方语言」，用契约/状态/可恢复组织并行 agent，用秒级内环/分钟级外环组织反馈，再用 law/metric/morality 三层和 Gate 保证每片都可不编造、可证伪、不越级。

---

## 数据出处

**仓库源码（`file:line`，已核实）**
- `Object::callp` 及脚本优先/原生回退：`core/object/object.cpp:768`、`:800-828`
- `MethodBind::ptrcall`：`core/object/method_bind.h:119`；结构哈希 `method_bind.cpp:35-45`
- GDExtension `object_method_bind_ptrcall`：`core/extension/gdextension_interface.cpp:1350`（注册 `:1843`）
- 声明式加载：`scene/resources/resource_format_text.h:148`、`packed_scene.h:273`
- 悬空引用校验：`gdsl/scene_json.cpp:173-193`（FR-004）
- `gdsl/abihash.h`（murmur3/fmix 移植）；`gdsl/godot_classes.h` + `gdsl/gen_godot_classes.py`（从 repo 根 `extension_api.json` 生成，1036 classes，D17）
- ABI 墙、Do-Not-Repeat：`.wolf/cerebrum.md`

**仓库文档（in-repo，source of truth）**
- `doc_ai/GODOT_LLM_DSL_DESIGN.md`（§2 两层架构、§3 语法、§4 快路径、§7 风险）
- `doc_ai/LLM_UNFRIENDLY_DESIGNS.md`（8 条不 AI native + 对策）
- `doc_ai/GDSL_LONG_RUN.md`（三件套、并行、Gate；数据 `JOURNEY.md:369-379`）
- `doc_ai/SOP_TDD_AI_TESTING.md`（§0.5 三层法、§4.4 DoD、§4.5 anti-drift）
- `AGENTS.md`「Parallel agent work」段（禁 worktree、文件所有权分区、stage 纪律、引擎构建串行化）
- `doc_ai/PLAN_GDSL_8H.md:46,52`（两条硬禁令）

**官方文档 / 规范（URL）**
- `.tscn` 格式：`docs.godotengine.org/en/stable/engine_details/file_formats/tscn.html`（无正式规范 issue #7102）
- 语言性能定性：`docs.godotengine.org/en/stable/about/faq.html`；typed-instructions +70%~150%：`godotengine.org/article/gdscript-progress-report-typed-instructions/`
- JSON Schema：`json-schema.org/specification`；tree-sitter：`tree-sitter.github.io/.../creating-parsers/2-the-grammar-dsl.html`
- VGDL：`github.com/GAIGResearch/GVGAI/wiki/VGDL-Language`；PuzzleScript：`puzzlescript.net`；ScriptDoctor：`arxiv.org/abs/2506.06524`
- GDC 2026 腾讯：`schedule.gdconf.com/session/ai-driven-3d-game-prototyping-with-engine-integration-presented-by-tencent-games-ai/917890`
- 自定义场景格式被硬编码阻塞：`github.com/godotengine/godot/issues/77397`
