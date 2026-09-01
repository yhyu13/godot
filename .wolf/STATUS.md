# STATUS — godot

> Single source of truth for resuming work. Read this FIRST when starting a session.
> Update this file at the end of every work phase so the next `/clear` resumes in 1 read.
> Last updated: 2026-09-01

---

## ✅ Done

<!-- Move items here from "🚀 Next phase" when finished. Group by area. -->

- LLM 友好 DSL 设计文档 `doc_ai/GODOT_LLM_DSL_DESIGN.md`（两轮 critic 收敛，零 blocking）
- 全局 skill `source-anchored-design`（research fan-out 门控 + 单 critic loop）
- 实现计划 `doc_ai/PLAN_LLM_DSL_IMPL.md`（D1/D2 已拍板）
- **parser 完整**：`parse_type_decl` / `parse_state_field` / `parse_type_block` / `parse_rule_block` / `parse_program`（单行产生式 + 多行 type 块 + rule/when/then + 整文件 Program AST）
- **声明式层**：`emit_tscn`（SceneSpec→.tscn，golden 逐字节）+ `scene_from_json`（JSON→SceneSpec + schema 校验，端到端 tracer bullet）
- **逻辑层 typechecker**：`typecheck`（AST→typed IR，拒绝重复类型名/未知字段类型/重复字段/未知触发类型）
- **逻辑层 codegen**：`emit_c`（typed IR→完整 GDExtension C 源，确定性）
- **S6.1 codegen 注册层**：`emit_c` 输出完整 GDExtension C 源——入口点 `gdsl_library_init` + SCENE 级 initialize/deinitialize + 每类型 `classdb_register_extension_class6` 注册（`GDExtensionClassCreationInfo6`）+ create/free 实例生命周期 + 状态字段落进 C struct（int→int64_t、float→double、Named→指针）。
- **S6.2a effect/ptrcall codegen v1**：`gdsl/effect.{h,cpp}`（guard/effect 解析器）+ typecheck 把 when/then 结构化进 TypedRule（并校验字段名存在于 by 类型）+ `emit_c` 把每条规则编译成可调用方法（`_impl`/`_call`/`_ptr` 三函数 + `classdb_register_extension_class_method` 注册）。ontology v1 = `set_field`（= / += / -=）+ 单比较 guard，self-only。生成 C 经 stub ABI 头 `cl /c` 语法编译通过。
- **S6.2b emit_signal codegen**：`emit(<signal>)` 零参信号发射。D5 阻塞解除——`Object::emit_signal` 兼容哈希 **2866548813 (0xAADC104D)** 从 Godot 源码逐条移植 murmur3/fmix（`gdsl/abihash.h`）计算得到（非手编）；codegen 走 `classdb_get_method_bind("Object","emit_signal",hash)` + `object_method_bind_call`，用 `get_variant_from_type_constructor(GDEXTENSION_VARIANT_TYPE_STRING_NAME)` 从 raw StringName 构造 Variant 封送，信号名 static StringName 缓存一次复用（`p_is_static=true` 免析构）。生成 C 经 stub ABI 头 `cl /c` 语法编译通过。
- **S6.2c 跨参与者 target codegen**：`rule <Name> by <Owner> [target <Type>]:` 显式声明碰撞对手方类型（D6）。`target.<field>` 解析进 Guard/Effect 的 `RefOwner::Target`，typecheck 按 ref 选 owner（self→by 类型 / target→target 类型）校验字段存在性并拒绝「引 target 却无 target 子句」。codegen 把规则方法扩成带 `GDExtensionObjectPtr p_target` 参数（`argument_count=1` + OBJECT `arguments_info`/class_name），经 instance binding（`object_set_instance_binding`/`object_get_instance_binding`）取回 target 的 C struct（D7：ABI 无 `object_get_instance`，binding 是唯一取回路径；free_callback 置 NULL 避免与 `free_instance_func` 双重释放）；`_call` 用 `get_variant_to_type_constructor(OBJECT)` 从 Variant 解 Object、`_ptr` 直接解 `*(GDExtensionObjectPtr*)p_args[0]`。生成 C 经 stub ABI 头 `cl /c` 语法编译通过。
- **测试**：83 用例 / 335 断言全绿；harness `gdsl/test.ps1` 13 文件（6 test + 7 源）
- **S6.3 真机集成 bug 修复（两个）**：① emit_signal 兼容哈希 **2866548813 → 4047867050 (0xF1458CAA)**——根因是 emit_signal 的 MethodInfo 有 Error 返回值（`object.cpp:1151` `_emit_signal` 返回 Error → `create_vararg_method_bind` 从签名推导 return type INT + class_name "Error"，`type_info.h:247-256`），手写 mi（`object.cpp:1866`）漏 return_val 导致哈希错 → `classdb_get_method_bind` 返 nullptr。真机 `--dump-extension-api` 对拍验证。② `GDExtensionPropertyInfo.hint_string` 传 NULL → 引擎 `PropertyInfo(const GDExtensionPropertyInfo&)`（`property_info.h:168`）无条件解引用崩溃；改成指向零初始化 `gdsl_empty_string[8]`。route B 真机跑通：`.gdsl` → 生成 C → `cl` 编 `.dll` → `.gdextension` → 引擎加载，初始化阶段全部通过。
- **退出阶段 segfault（EXIT 139）确认修复（2026-09-01 实测）**：最小复现（加载含方法 GDExtension + 退出）在官方 rc3（`.godot-bin/`）与 fork release-editor 构建上都 exit 0（各 5/5 连跑）。修复本体 = 54864dd（`_unregister_extension_class` 无条件 `_clear_extension`，`gdextension.cpp:744`）。
- **TASTE SCORE（2026-09-01 新增）**：`doc_ai/TSCORE.md` 标准 + `gdsl/toolchain/tscore.py` scorer（秒级、无 LLM、无引擎）。基线 T=97.8（C/V/E=1.0 饱和，Q=0.889 唯一区分度）。诚实的下一步：加难任务集重启 C/E、建 mutation harness 测 V（FR-005）。
- **unload→free 崩溃 = 非 editor 模式 artifact（2026-09-01 修正）**：早前误判「带修复仍崩」——实为 `--headless --path`（非 editor）跑出。链：`main.cpp:2222-2224` reloadable 仅 editor 开启 → `loader:217` reloadable=false → `gdextension.cpp:557-560` 追踪关闭 → `instances` 空 → `_clear_extension`(:744) 清空气 → `p.free()` 撞已卸载 DLL。非引擎 bug，勿作验收。记录 `doc_ai/SEGFAULT_UNLOAD_REPRO.md`、复现 `gdsl/toolchain/repro_unload_crash.ps1`。

---

## 🚀 Next phase

**Active spec:** `009-engine-integration`（SDD phase `specify`，见 `specs/009-engine-integration/spec.md`）

**Goal:** S6 引擎集成（route B = GDExtension，设计文档 §2 定死）。route B 已跑通（`.gdsl` → 生成 C → `.dll` → `.gdextension` → 引擎加载，两个真机 bug 已修）。**退出阶段 segfault（EXIT 139）已确认修复**：普通退出最小复现（加载含方法 GDExtension + `--headless --quit`）在官方 rc3 与 fork 构建上都 5/5 exit 0（见 Done）。下一步：009 引擎集成完整验收（节点/连接数 == 配方 + rule effect 经 ptrcall 生效可观测）+ 010 LLM 接口度量（FR-006/007/008 harness）——后者才是「LLM 友好」的实证，目前是空的。

### Acceptance criteria
1. ~~`emit_c` 补跨参与者 `target` 参数 codegen~~ ✅ S6.2c done（77 用例 319 断言绿）
2. 生成 C → `make_interface_header.py` 生成 `gdextension_interface.h` → `cl` 编成 `.dll` → `.gdextension` manifest
3. 一份 `.gdsl` 经完整管线（parser→typecheck→emit_c→编译→引擎加载）跑出真场景

### Files to create / edit
| Type | File | Content |
|---|---|---|
| new | `gdsl/toolchain/` | 生成 `gdextension_interface.h` + `cl` 编 `.dll` + manifest 的脚本 |
| new | `gdsl/example/` | 示例 `.gdsl` + `.gdextension` manifest + 最小测试工程（.tscn） |

### Closed decisions
- DSL 架构：声明式走 `ResourceFormatLoaderText`、逻辑走 GDExtension `ptrcall`
- D1：编译器内核独立 `gdsl/` + 独立 doctest（秒级 red-green），S6 再集成
- D2：parser 先行（用户拍板，覆盖「声明式 tracer bullet」建议）
- S2 JSON 解析：手写最小 JSON 解析器 `gdsl/json.{h,cpp}`，内核零第三方依赖
- D3（S6.1）：codegen 用 `classdb_register_extension_class6`（4.7 最新版 ABI，`GDExtensionClassCreationInfo6` + `GDExtensionClassCreateInstance3`），ABI 符号逐条核对 `core/extension/gdextension_interface.json` + `gdextension_interface_header_generator.cpp`
- D4（S6.2a，effect ontology v1）：effect 动词从 `set_field`（`ref.field = / += / -= literal`）起步；guard = 单比较 `ref.field <cmp> literal`（`> >= < <= == !=`）；`ref` 仅 `self`（或裸字段名）。规则编译成方法经 `classdb_register_extension_class_method` 注册（call_func + ptrcall_func 双路径）。`emit_signal` 留 S6.2b、跨参与者 `target` 留 S6.2c（不静默忽略，明确拒绝）。
- **D5（S6.2b，emit_signal 落地）**：GDExtension ABI 无 `object_emit_signal`，走 `classdb_get_method_bind("Object","emit_signal",hash)` + `object_method_bind_call`。哈希不手编——从 Godot 源码逐条移植 `hash_murmur3_one_32`+`hash_fmix32`（`core/templates/hashfuncs.h:112-122,144-150`）与 `MethodInfo::get_compatibility_hash`（`core/object/method_info.cpp:87-116`）进 `gdsl/abihash.h`，对 `emit_signal` 的 MethodInfo（`core/object/object.cpp:1864-1870`：1 个 STRING_NAME=21 参数、vararg、无返回、无默认参数）计算得 **2866548813 (0xAADC104D)**。Variant 封送用 `get_variant_from_type_constructor(GDEXTENSION_VARIANT_TYPE_STRING_NAME)`（4.7 ABI 已无 `variant_new_string_name`，只剩 `variant_new_copy/nil` + 通用 `variant_construct`）；信号名用 static StringName（`p_is_static=true`）缓存一次复用，规避「无 `string_name_destroy` API + 每发射一次 StringName 会无界泄漏 static_count」。

- **D6（S6.2c，target 类型来源）**：`rule <Name> by <Owner> [target <Type>]:` 显式声明碰撞对手方类型（不用默认与 by 同型、也不用固定 Node——前者等于 self 无意义、后者丢类型安全）。`by` 仍是规则所有者（self），`target` 是独立声明；引 `target.<field>` 却无 target 子句 → typecheck 拒绝。
- **D7（S6.2c，target 取回）**：GDExtension ABI 有 `object_set_instance`（写 self 的 extension instance，引擎 `_get_extension_instance()` 读）但**无 `object_get_instance`**（grep json 零命中）——跨对象取回 C struct 唯一路径是 instance binding `object_set_instance_binding`/`object_get_instance_binding`（token 键）。create_instance 把 self 同时挂到 binding（free_callback 置 NULL，内存由 `free_instance_func` 统一释放，避免双重释放）；规则方法经 `object_get_instance_binding(p_target, gdsl_library, &callbacks)` 取回 target struct。方法带 1 个 OBJECT 参数（`argument_count=1` + `arguments_info` class_name=target 类型），`_call` 用 `get_variant_to_type_constructor(GDEXTENSION_VARIANT_TYPE_OBJECT=24)` 从 Variant 解 Object、`_ptr` 用 `*(GDExtensionObjectPtr*)p_args[0]`。

### Open decisions
- 逻辑层编译 vs 热重载取舍（重编译 vs 做进引擎核心走路线 A）——route B 已按设计文档 §2 拍板，此条仅在「需要热重载」时重开。

---

## 📁 Active architecture

- **Stack:** 独立 C++17 内核（MSVC + doctest 单头），零 Godot core 依赖
- **Key modules:** `gdsl/parser` → `gdsl/typecheck` → `gdsl/effect` → `gdsl/codegen_logic`；声明式 `gdsl/json` → `gdsl/scene_json` → `gdsl/codegen_declarative`
- **Patterns:** 纯函数 seam + 先红后绿 + 确定性输出

---

## ⚠️ External blockers (don't block coding)

- scons **已装**（4.11.1，`uv tool install scons`）——二进制在 `C:\Users\XINDONG\.local\bin\scons`（不在 PATH，用绝对路径或 `export PATH="$HOME/.local/bin:$PATH"`）。真机集成唯一硬前提 = 一个 Godot 引擎二进制（已就位于 `.godot-bin/`，已 gitignore）。
- **退出阶段 segfault：已修复**（普通退出 5/5 exit 0，见 Done）。非 editor 模式的 unload→free 崩为 artifact（reloadable 仅 editor 开启），不作阻塞。详见 `doc_ai/SEGFAULT_UNLOAD_REPRO.md`。

---

## 🔧 Useful commands

```bash
cd gdsl && powershell -File test.ps1   # 编译 + 跑 77 用例（秒级）
export PATH="$HOME/.local/bin:$PATH" && scons platform=windows target=editor -jN  # scons 已装（4.11.1）
```

---

## 📚 References (read IF needed)

- `.wolf/cerebrum.md` — User Preferences + Do-Not-Repeat + Decision Log
- `.wolf/anatomy.md` — token-efficient file index
- `.wolf/buglog.json` — known bugs + fixes
