# GDSL 8 小时攻坚计划 —— 只修退出 segfault（EXIT 139）

> 上一版把「修 bug」当 2 小时前置、后面接引擎集成，顺序错了。
> 这个 bug 就是引擎集成路径上的拦路虎：场景加载完、断言没跑完，进程退出就崩，
> 009 的 [SceneTree] 断言根本跑不完整。所以 8 小时全部花在这个 bug 上。
> 只有退出干净了，才谈得上 009。

## 诊断结论（本会话源码坐实，不是猜）

崩点：退出阶段 segfault，EXIT 139，CRT 静态析构阶段。

机制：**引擎卸载顺序错位**。

- `register_core_types.cpp:433` `memdelete(gdextension_manager)` 先跑
  → `Ref<GDExtension>` 引用归零 → `GDExtension::~GDExtension()`（gdextension.cpp:877）
  → `close_library()` → FreeLibrary，DLL 被卸载
- 之后 `:488` `ObjectDB::cleanup()`、`:495` `ClassDB::cleanup()` 才跑
- GDSL 对象若到退出仍存活，`ObjectDB::cleanup()` 调 DLL 里的 `free_instance_func`
  （如 `player_free_instance`）——DLL 已卸，跳进已卸载内存 → 崩

推翻的旧假设（JOURNEY Era 17 / STATUS 里写的）：「GDExtensionMethodBind 存着 DLL
函数指针、DLL 先卸载 → MethodBind 析构悬空」——不对。MethodBind 的虚表/析构在引擎
二进制里，name/arguments 都被引擎拷贝（gdextension.cpp:196,212），销毁 MethodBind 不
会碰 DLL。DLL 里真正会晚调用的指针是 `free_instance_func` / `call_func` / `ptrcall_func`
这三个「被调用」而非「被销毁」的东西。

## 关键路径（全部串行，只有 bug 修完才动引擎集成）

控制实验 → 抓真栈 → 定 fix → 落地验证 → （尾巴）009 第一条验收

---

## 阶段 0（0–1h）：控制实验，锁定「谁的 bug」

目标：分清「上游 Godot 卸载顺序 bug」还是「GDSL 生成代码特有的问题」。

动作（按顺序）：
1. 复现：`.godot-bin/` 的 `.console.exe` 跑最小样例，`--headless --quit`，确认 EXIT 139 稳定复现
2. 对照 A：type-only 的 `minimal.c`（无方法、只注册类型）退不退出崩？→ 区分「方法注册触发」还是「存活实例触发」
3. 对照 B：一个库存/官方 GDExtension C 样例（注册 class + method + 有实例）退出崩不崩？
   - 也崩 → 上游卸载顺序 bug，改 `register_core_types.cpp` 顺序（本 fork 合法改动）
   - 不崩 → GDSL 生成代码特有（如对象泄漏没在关门前释放），diff 出差异点

Gate：两个对照各有一个明确答案（崩 / 不崩），写下来。

禁止：还没做对照就定 fix。

## 阶段 1（1–2h）：抓真栈（落实 Do-Not-Repeat 教训）

目标：一条带符号的调用栈，坐实崩在哪个调用——`free_instance_func`、还是 method call、
还是别的。不拿栈不写 fix。

动作（三条路并行，哪条通走哪条）：
- gflags `+hpa`（page heap）+ AEDebug，让裸 segfault 落 dump
- WER LocalDumps 注册表 + 重跑，拿 `.dmp`
- cdb `-g` 无头挂 + 加长 timeout（之前交互卡住，别用交互）

Gate：拿到带符号栈，说清「崩在哪个函数、哪个模块、卸载顺序对不对」。

## 阶段 2（2–4h）：定 fix + 落地

按阶段 0/1 的结论二选一：

- 若是上游卸载顺序 bug：改 `core/register_core_types.cpp`，把 `memdelete(gdextension_manager)`
  挪到 `ObjectDB::cleanup()` + `ClassDB::cleanup()` 之后。本仓库是 fork（yhyu13/godot），
  这是合法改动，不是改不可动的上游。
- 若是 GDSL 对象泄漏：查泄漏点（instance binding 配对 / free_instance_func 是否被调 /
  场景卸载时对象是否释放），修 codegen。

Gate：四个样例（self_rule / target_only / emit_only / combo）退出全部 exit 0，无 139。

## 阶段 3（4–6h）：回归 + 反补

- 跑 `gdsl/test.ps1`（83/335）确认 codegen 没被改坏
- 真机四样例 + 最小真场景加载→退出，全干净
- 若卸载顺序是通用 bug：写一条最小复现说明（不只为 GDSL 复现），标注这是引擎级问题

Gate：83/335 全绿 + 四样例退出干净 + 复现说明落盘。

## 阶段 4（6–8h）：只在 bug 修完后，碰 009 第一条验收

- 009 的 FR-003：`tests/scene/` 写 `[SceneTree]` doctest，断言实例化节点数/连接数 == 声明 JSON
- 这是引擎集成，但 bug 修完、退出干净后，它才是可跑的

Gate：009 第一条 acceptance 绿；没修完 bug 就不碰这条。

---

## 风险（诚实标注）

- 阶段 1 抓栈是最大风险点——JOURNEY 记录 cdb 卡、procdump 不落盘、WER 没 dump，
  三条路都可能继续失败。备选：直接按阶段 0 的对照结论改卸载顺序（若对照 B 证明是上游
  bug，源码证据已足够强，可以边改边用真机退出验证替代栈）
- 阶段 2 若走引擎改动，要小心不引入别的 teardown 顺序问题（ClassDB::cleanup 之前必须
  已经没对象引用 GDExtension 的东西）
- 最小样例若连对照 A（type-only）都崩，说明问题比「存活实例」更早，回到阶段 0 重新定位

## 参考

- 崩点机制：`core/register_core_types.cpp:433/488/495`、`core/extension/gdextension.cpp:877`
- MethodBind 拷贝证据：`core/extension/gdextension.cpp:49-239`（尤其 196、212）
- 生成代码：`gdsl/codegen_logic.cpp`（free_instance_func 在 248、方法注册在 354-388）
- 现状/历史：`.wolf/STATUS.md`、`JOURNEY.md` Era 17/18、`specs/009-engine-integration/spec.md`
