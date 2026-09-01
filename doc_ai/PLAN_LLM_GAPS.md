# PLAN — gdsl LLM 友好性两缺口（coercion + 单一事实源）

> 一句话：关掉上轮审计剩下的**两个真缺口**——① typecheck 不校验「字面量 vs 字段类型」（`then target.hp -= 0.5` 打 int 字段会静默通过）；② `@extends` 基类与场景节点 `type` 不校验 ClassDB（LLM 能编 `@extends Nonsense`）。MVP 只做 ①，纯 red-green，零新文件零引擎。
> **纠正**：上轮说的第 8 条「悬空引用缺校验」是错的——`scene_json.cpp:173-193` 已有（FR-004）。真实缺口只有 2 个。

---

## 0. 目标（persist，全程不窄化）

让 gdsl 内核的「LLM 友好性」补上最后两个静态闸门，使 LLM 输出「类型不对」或「引用不存在的类」都在编译期被拒，而不是生成 C 后 UB / 运行时炸。

- 验收（成功判据）：
  1. Gap A（coercion）：所有「字面量类型与字段类型不兼容」的输入被 typecheck 拒绝；兼容的（int→float 拓宽）通过。catch rate 100%（见 benchmark）。
  2. Gap B（ontology）：`@extends` 基类、场景节点 `type` 不在 ClassDB 名表 → 拒绝；合法名通过。
  3. 全量回归：`gdsl/test.ps1` 现有 83 用例仍全绿 + 新增 red→green 用例。
  4. 不碰引擎 core、不碰 codegen_logic 的注册/退出路径（segfault 关键路径零改动）。

---

## 1. 修改范围

**touch（允许改）**
- `gdsl/typecheck.cpp` / `gdsl/typecheck.h` — 加字面量/类型校验（Gap A）、加 `@extends` 基类校验（Gap B）。
- `gdsl/test_typecheck.cpp` — 加 red→green 用例（Gap A/B 逻辑层）。
- `gdsl/scene_json.cpp` — 加节点 `type` 名校验（Gap B 声明式层）。
- `gdsl/test_declarative.cpp` — 加节点 `type` 校验用例（Gap B）。
- 新：`gdsl/toolchain/gen_classdb.py`（从 `extension_api.json` 抽类名 → 生成 `gdsl/classdb_names.h`）+ 生成头 `gdsl/classdb_names.h`（Gap B）。
- `gdsl/test.ps1` — 仅当新增 `.cpp` 文件才改编译行。

**not touch（禁区，越界即 scope violation）**
- 引擎 core/scene/servers/editor/modules（`typecheck`/`scene_json` 之外的任何引擎源码）。
- `gdsl/codegen_logic.cpp` / `codegen_declarative.cpp` — 生成逻辑与注册/退出路径（segfault 关键路径）。
- `gdsl/abihash.h` — 哈希黄金值 4047867050 不动。
- `specs/`、`docs/`、`.wolf/` 运行时状态、`doc_ai/` 设计文档（除已做的 #8 纠正）。

---

## 2. 详细计划（垂直切片，一个测试一个实现）

### Slice 1（MVP）— Gap A：字面量/类型 coercion 政策

**问题（已核实）**：`typecheck.cpp` 校验字段存在（`:126-129,145-148`）与类型名（`:84-88`），但**不校验字面量 vs 字段类型**：
- `state: hp: int = 3` 的 `default_value` 原样拷贝（`:83`），`hp: int = "hello"` 也过。
- `guard.value`/`effect.value` 是裸字符串（`effect.h:17,27`），`then target.hp -= 0.5` 打 int 字段 → typecheck 通过 → 生成 `hp -= 0.5`（int64_t 截断）。

**政策（钉死，写进 typecheck 注释 + 测试）**：
- 无隐式转换，仅允许 **int 字面量 → float 字段**（拓宽）；其余窄化/跨类一律拒绝。
- Int 字段：字面量必须是整数（含 `-`，不含 `.`/`e`）；`0.5`、`"x"`、`true` 拒。
- Float 字段：int 或 float 字面量皆可。
- Bool 字段：仅 `true`/`false`。
- String 字段：v1 规则层不涉及（guard/effect 只允许数值字面量），default 允许任意字符串。
- Named 字段：guard/effect 里对 Named 字段赋字面量 → 拒（v1 不支持对象字面量）。

**实现**：`typecheck.cpp` 加 `static bool literal_matches(ValueType, const std::string &literal, std::string &err)`，在 Pass 2（default_value）、Pass 3（guard.value / effect.value，按 ref 选 owner/target 拿到字段类型）三处调用。

**red→green**（`test_typecheck.cpp` 先写红）：
- 红：`hp: int = 0.5` 拒；`then target.hp -= 0.5`（hp:int）拒；`then self.hp = "x"` 拒；`then self.flag = 1`（flag:bool）拒。
- 绿：`speed: float = 400.0` 过；`then self.speed = 3`（int→float）过；`then self.hp -= 1` 过。

### Slice 2 — Gap B：ClassDB 单一事实源（`@extends` + 节点 `type` 校验）

**问题（已核实）**：`typecheck.cpp:73` `tt.base = t.base` 原样拷贝，`@extends Nonsense` 不校验；`scene_json.cpp:56` `out.type = type->string_value` 原样拷贝，节点 `type: "NoSuchNode"` 不校验。effect 动词是手写 enum（`effect.h:21`）。

**实现（MVP 面 = 类名表校验）**：
1. 新脚本 `gdsl/toolchain/gen_classdb.py`：读 `extension_api.json`（6.96 MB 全 ClassDB 导出），抽 `classes[].name` 生成 `gdsl/classdb_names.h`（排序后的 `const char* const kClassNames[]`，零第三方依赖，内核可 `std::lower_bound` 查）。
2. `typecheck.cpp` Pass 2：`@extends` 基类名查表，查不到 → `unknown base class 'Nonsense'`。
3. `scene_json.cpp` `map_node`：节点 `type` 查表，查不到 → `unknown node type 'NoSuchNode'`。

**red→green**：`@extends Nonsense` 拒 / `@extends CharacterBody2D` 过（在表里）；节点 `type: "NoSuchNode"` 拒 / `type: "Node2D"` 过。

**defer（非本计划）**：effect 动词、signal 名、字段类型的全量 ontology 从 extension_api.json 生成——需要抽取 property/method/signal 完整模式 + 新 ontology spec，量太大，单独起 spec。

---

## 3. MVP 定义

**MVP = Slice 1（Gap A 字面量/类型 coercion）**，理由：最小垂直切片、零新文件、零引擎、纯 red-green，直接补「LLM 写错类型」这一最高频坑。交付 = typecheck 拒绝不兼容字面量 + 全部现有 83 用例仍绿 + 新增红绿用例。Slice 2 是第二个可独立交付的切片。

---

## 4. Benchmark（度量「好」，不是感觉）

- **Gap A 基准**：一组 mutation 输入（float→int、string→int、bool→int、`0.5` 打 int、`"x"` 打 int、int→float 合法、合法全部）的 **catch rate = 拒绝数 / 应拒数 = 100%**，且合法输入 0 误杀。落地成一个 doctest 用例组，数字由 `--test` 输出断言数体现。
- **Gap B 基准**：类名表覆盖 `extension_api.json` 全类数（如 ~700+ 类），`@extends`/节点 type 的 mutation（编造类名）100% 拒绝、合法类名 100% 通过。
- 均与现状对比：现在这些坏输入 100% 静默通过（typecheck 无此闸门）。

---

## 5. 依赖 / 风险（诚实标注）

- 无引擎依赖、无第三方依赖（除 Gap B 的生成脚本用 Python 标准库读 json，不进内核运行时）。
- 不触碰 segfault 关键路径，故本计划与「退出阶段 segfault」完全解耦，可并行推进。
- Gap B 的 `extension_api.json` 键名（`classes[].name`）在实现时对拍一次，别凭记忆。
- 唯一决策待拍：coercion 政策「仅 int→float 拓宽」是否足够（v1 是否需要 int↔bool、string 规则层参与）——默认按最严，用户可放宽。
