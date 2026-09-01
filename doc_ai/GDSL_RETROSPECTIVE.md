# GDSL 开发复盘 —— 为什么做 / 怎么做的 / 得失 / 如何用好 Agent / 下次更快

> 基于 gdsl 全程开发记录（JOURNEY Era 1-27 + cerebrum D1-26）的诚实复盘。
> 目的不只是回顾，而是提炼「如何驾驭 agent 做这类引擎/语言开发」的方法，并留下下一次少走弯路的清单。

---

## 一、为什么开发 gdsl

**一句话：Godot 本身对 LLM 不友好，gdsl 是给 LLM 用的「配方语言」，把「写代码」降维成「填配方」。**

动因来自 ERA 19 的审计——Godot 的「不 AI-native」设计：

- **GDScript 是命令式 + 动态类型**：Variant 满天飞，LLM 写错率天然高。
- **字符串寻址**：`get_node("path")`、`connect("signal")`、属性名全是字符串，幻觉成灾。
- **`.tscn` 无 schema**（issue #7102）：结构化数据没有校验层。
- **三份 API 表示会漂移**：C++ 宏绑定 vs `extension_api.json` vs `doc/*.xml`。
- **MethodBind 按结构哈希匹配**（不按名字）：手写哈希错一字节就查不到方法（D5/D11 的 2866548813→4047867050 就是踩这个）。

所以 gdsl 的反向设计（D 系列决策）：

- **配方而非程序**：`type X @extends Base / state / rule when-then` 小语法，可写错的空间小。
- **可校验 > 可表达**：reject-not-retry——幻觉在编译期被拒成「看得到的报错」，而不是运行时静默崩。
- **固定 effect ontology**(D4)：`set_field / emit`，LLM 不能发明符号。
- **确定性输出**(D3)：同一输入同一 C 源，格式化噪声无关。
- **文本进 → native 出**：`.gdsl` 文本 → 生成 C → 编 DLL → GDExtension，LLM 只见文本。
- **两层**：声明式(JSON→tscn 场景) + 逻辑(配方→GDExtension 类)。

**真正目标(ERA 16 说的)**:让 LLM 能**用**这个语言做出游戏——不是「编译器正确」，是「LLM 友好」+「做得出游戏」。这两件事是这次开发的验收线。

---

## 二、开发历程（按主题，非严格时间序）

1. **起点**：浅克隆 godot fork → AGENTS.md/CLAUDE.md（把构建体系压成带 `file:line` 的可核事实）。
2. **设计**：`doc_ai/GODOT_LLM_DSL_DESIGN.md`（两层架构 + 37 个源码锚点），两轮 critic 抓出「route A GDExtension 不可达」等实质错。
3. **内核**：parser → 类型系统 → 声明式/逻辑 codegen，纯 doctest 秒级红绿（D1 独立目录，快周期优先）。
4. **S6 集成**：codegen 扩到真 GDExtension（注册/实例生命周期/方法/属性/信号），ABI 无 `variant_new_string_name`、无 `object_get_instance`、无 `string_destroy`——全是源码核实后绕过/记录。
5. **真机**：`.godot-bin` 引擎就位，修掉两个真 bug（emit_signal 哈希 + hint_string NULL 崩溃）。
6. **segfault 攻坚(Era 20)**：真机退出 EXIT 139。源码钉死卸载顺序 + 上游 #98182 对拍，两条修复方向实测撞墙，最后定位为 **dev-build 特有、release 干净、editor-only artifact**——这是**最大的弯路**（详见「失」）。
7. **热重载(Direction 5)**：getter/setter 属性 + recreate_instance + manifest `reloadable=true`，跨热重载保住 hp/owner/nickname（真机）。
8. **LLM 友好性缺口**：Gap A（字面量类型 coercion）+ Gap B（`@extends` 基类查 `godot_classes.h`，从 `extension_api.json` 生成）。
9. **010 LLM 收敛度量**：cold-test（LLM 只看语法+few-shot，禁工具）→ `gdslc` 判有效 → first-try rate/iterations-to-valid/few-shot 敏感性。**16/16 first-try**（8 任务 × K{0,1}）。
10. **FR-011 playtest 闭环**：配方 → DLL → 真引擎跑 → 断言规则真的执行。
11. **语义金标准**：对着「正确值」断言（非配方自己的 effect，后者抓不到写反）→ 写反被抓。
12. **semfix 回喂**：写反 → 金标准报错 → LLM 翻回 → ALLPASS。
13. **009 场景集成**：配方 → DLL → tscn → 真引擎加载 → TC-1..8（加载/类/状态/规则/跨实体/信号/生命周期/游戏轨迹）全过。
14. **声明式路径**：JSON → `scene_from_json` → `emit_tscn` → 真机实例化（BUL_CLASS=Bullet、damage=2、hp=10）。

**产出**：`gdsl/` 编译器内核（parser/typecheck/effect/codegen + 97/403 单测）+ `gdsl/toolchain/` 全套验证工具链（llm_conv_bench/playtest/golden/semfix_loop/scene_accept/scene_lifecycle）+ 过程文档（JOURNEY/PLAN/设计）+ 真机证据。

---

## 三、得（做对的 / 沉淀的）

1. **可执行源优先**：AGENTS.md、设计文档、ABI 结论全部钉 `file:line`，复验零漂移、对拍抓到过时假设（Era 3「platform 必填」自纠）。
2. **critic loop（fresh-context 非自审）**：两次设计 review + subagent critic 抓到实质架构错，没照抄 critic、逐条 grep 复核才改。
3. **诚实的边界报告**：把「编译有效」与「运行时正确」、「已测」与「未测」、「verify/unverified/unknown」分得清清楚楚——不把 `cl /c` 语法通过当「信号真的发了」。
4. **秒级红绿**：D1 独立 doctest harness 把周期压到秒级（不依赖 scons/引擎），vert-slide 一步一测。
5. **通过 ABI 墙**：v0 遇到无 `variant_new_string_name`/`object_get_instance`/`string_destroy`——源码核实后绕过或记录为已知限制，不假装能 engineering around。
6. **真机证据才是终点**：playtest/009 全是真引擎跑出来的（不是代码生成断言），这才让「LLM 友好」「做得出游戏」有了数字。
7. **llm 友好度量有数**：16/16 first-try、8/8 金标准验证、写反被抓且 LLM 修对——这些是「能不能用」的直接证据。
8. **可复用资产**：一套「编译→真跑→语义→回喂→场景」工具链，别的引擎 DSL 可套。

## 四、失（弯路 / 坑 / 代价）

1. **segfault 弯路最大**：真机 EXIT 139 啃了很久（抓栈三路全堵：cdb 卡、procdump 无 dump、WER 不落盘），两条修复方向实测撞墙，最后定位 dev-only/editor-only artifact。**投入与产出严重不符**——如果早点用 release 版跑对照（Era 20 后面发现的），能省大量时间。
2. **过度向用户确认**（「为什么你要停」）：把 software-dev-loop 的 per-round gate 当成硬停点 + 钱犹豫 + 套用 lane 边界 + 要许可习惯——**全是可改行为，非技术瓶颈**。Hermes 核心其实是 act_dont_ask。
3. **GDScript 生成器 bug 反复 3 次**：`var` 重声明（每 scenario/rule 重复声明 `owner`/`scenario_ok`/`before`/`after`）→ GDScript 解析错 → autoload 不跑 → 引擎 240s 超时。字符串值没引号、FAIL 信息把值内插进双引号字符串——同类。**烧了 3 次 240s 才治本**（应生成前静态扫 `var` 重声明 + 值当独立参数）。
4. **金标准是我从任务描述推的，不是用户定的**：能验「配方跟我的理解一致」，但「我的理解对不对」靠不上——这是循环，语义正确性最终要人。
5. **LLM 运行成本**：`claude -p` 每次 ~$0.21（42K base prompt 省不掉）；全流程跑下来 ~$6-8。值，但要预算化 + 复跑取均值才稳。
6. **lane 边界混乱**：009 部分被我过度标为「另一 agent」，其实只有 segfault/引擎退出路径是；声明式/逻辑/工具全是我的 lane。不清导致有时不敢动手。
7. **声明式与逻辑层是两套**：到 009 才接起来（recipe→DLL 类三塔 vs JSON→tscn 声明式），集成成本后置了。

---

## 五、我们怎么用好 agent（方法论）

1. **把「纪律」写进任务，不靠口头**：AGENTS.md 两次都自带「Prefer executable sources of truth over prose」「When in doubt, omit」——这些是验收标准，不是建议。
2. **给可执行源 + 接受标准，让 agent 对拍**：agent 自己会推翻你的过时假设（`platform` 必填、MultiplayerReplicator、nanosvg→ThorVG…），前提是它有 source 可查。
3. **critic 要 fresh-context，别自审**：自审会顺着自己的假设圆；一个不知道你思路的 critic 才能抓到架构级错。
4. **要求 agent 诚实分「已验/未验/未知」**：把「编译过」和「跑对了」分开，把「没测」直说——不粉饰才有真进展。
5. **用小步 + 可执行产物推进**：vert-slide、一条命令一个验证，别让 agent 攒一大堆再交。
6. **让 agent 对「好」有定义**：`"it works" 不是结果，baseline 数字才是`——LLM 友好用 first-try rate，不用「应该很友好」。
7. **别把 agent 当「执行工具」，当「带证据的工程师」**：它能在对拍中自纠、能报告「我先写错了」、能跳过死路（direction B 撞墙立刻换）。
8. **lane 内放开跑，真判断才问**：有合理默认就推进做完再报；只有真正需要人的（如金标准语义）才停。

## 六、下一次开发 gdsl（或类似）怎么更快更好

**这次的弯路清单 → 下次的检查单：**

1. **先定验收线再动工**：把「LLM 友好」的度量（first-try/iterations/语义对 spec）从一开始就定成硬门，别到 010 才想起来。
2. **先跑对照组再深挖**：遇到"真机 bug"先跑 release 版 + 最小对照（segfault 就是 dev-only，早对照早止损）。
3. **生成 GDScript/GDScript 用共享 helper，一次治本**：一个 `gen_gd.py` 内建「声明一次、值当独立参数、字符串加引号」三条纪律 + 每生成前静态扫 `var` 重声明——别再烧 3 次 240s。
4. **金标准要用户早参与**：任务定义时就把期望运行结果写进 spec，别等 agent 从描述推——语义正确性是人的判断，不是 agent 的自洽。
5. **声明式 + 逻辑层从设计第一天就打通**（recipe→DLL 类 与 JSON→tscn 是两套，集成成本后置会拖到 009）。
6. **LLM 度量成本预算化**：固定 N 次 × M 任务,复跑取均值;`claude -p` 每 call ~$0.21,先算总预算。
7. **单事实源 upfront**：Godot 类名表 → 生成 `godot_classes.h`(D17)是对的;版本漂移时重生成脚本跑一次。
8. **自治配置提前设**：`approvals.mode smart` + loop 的 `next step: auto-start`——别让 consent gate/「wait-for-user」打断流程。
9. **工具链沉淀成 skill**:把「编译→真跑→语义→回喂→场景」闭环 + 那些坑(ABI 墙/GDScript 生成/Windows cl 引号/GFW push 重试)打包,下一个引擎 DSL 直接复用。
10. **诚实是最高效**:报告「未验/未知/想多了」能让下个 agent/人立刻知道哪里不能信,省得重蹈。

---

## 结论

gdsl 从「一个念头」到「一个能被 LLM 一次写对、能编译、能真引擎跑、语义能被校验、写反能被回喂修对的配方语言」——**这条链是真闭环、有真证据的**。

最大的得：一套**可复用的「LLM 友好语言 + 验证工具链」方法论**，以及**诚实地知道什么已被证明、什么没有**。
最大的失：segfault 弯路的投入、反复问用户确认、GDScript 生成坑烧的 3 个 240s、金标准缺用户参与。
这些失的共性：**都是「过早收敛 / 不够早对照 / 不够早让人参与」的可避免成本**——正好是下一份检查单要堵的。
