# STATUS — godot

> Single source of truth for resuming work. Read this FIRST when starting a session.
> Update this file at the end of every work phase so the next `/clear` resumes in 1 read.
> Last updated: 2026-08-29

---

## ✅ Done

<!-- Move items here from "🚀 Next phase" when finished. Group by area. -->

- LLM 友好 DSL 设计文档 `doc_ai/GODOT_LLM_DSL_DESIGN.md`（两轮 critic 收敛，零 blocking）
- 全局 skill `source-anchored-design`（research fan-out 门控 + 单 critic loop）
- 实现计划 `doc_ai/PLAN_LLM_DSL_IMPL.md`（D1/D2 已拍板）
- **parser 完整**：`parse_type_decl` / `parse_state_field` / `parse_type_block` / `parse_rule_block` / `parse_program`（单行产生式 + 多行 type 块 + rule/when/then + 整文件 Program AST）
- **声明式层**：`emit_tscn`（SceneSpec→.tscn，golden 逐字节）+ `scene_from_json`（JSON→SceneSpec + schema 校验，端到端 tracer bullet）
- **逻辑层 typechecker**：`typecheck`（AST→typed IR，拒绝重复类型名/未知字段类型/重复字段/未知触发类型）
- **逻辑层 codegen**：`emit_c`（typed IR→C 结构体 + 规则注释，确定性）
- **测试**：35 用例 / 125 断言全绿；harness `gdsl/test.ps1` 扩到 11 文件（4 test + 7 源）

---

## 🚀 Next phase

**Goal:** S6 引擎集成——把 `emit_c` 扩成真 GDExtension 代码（注册 + effect + ptrcall），接 `modules/gdsl/`，scons 构建后跑真场景。前置：装 scons + 全量引擎构建（当前环境无 scons、无内置二进制）。

### Acceptance criteria
1. `emit_c` 生成可编译的 GDExtension C（非当前 struct/注释骨架）
2. `modules/gdsl/` 模块注册进引擎，scons 构建通过
3. 一份 `.gdsl` 经完整管线跑出真场景

### Files to create / edit
| Type | File | Content |
|---|---|---|
| edit | `gdsl/codegen_logic.cpp` | 扩到真 GDExtension 注册/effect/ptrcall |
| new | `modules/gdsl/` | SCsub + register_types + config.py |
| new | 示例 `.gdsl` + `.gdextension` + 测试场景 | 端到端真场景 |

### Closed decisions
- DSL 架构：声明式走 `ResourceFormatLoaderText`、逻辑走 GDExtension `ptrcall`
- D1：编译器内核独立 `gdsl/` + 独立 doctest（秒级 red-green），S6 再集成
- D2：parser 先行（用户拍板，覆盖「声明式 tracer bullet」建议）
- S2 JSON 解析：手写最小 JSON 解析器 `gdsl/json.{h,cpp}`，内核零第三方依赖

### Open decisions
- effect ontology 覆盖边界（设计文档 §7 最大风险）
- 逻辑层编译 vs 热重载取舍（重编译 vs 做进引擎核心走路线 A）

---

## 📁 Active architecture

- **Stack:** 独立 C++17 内核（MSVC + doctest 单头），零 Godot core 依赖
- **Key modules:** `gdsl/parser` → `gdsl/typecheck` → `gdsl/codegen_logic`；声明式 `gdsl/json` → `gdsl/scene_json` → `gdsl/codegen_declarative`
- **Patterns:** 纯函数 seam + 先红后绿 + 确定性输出

---

## ⚠️ External blockers (don't block coding)

- S6 需 scons + 全量引擎构建（当前环境未装）

---

## 🔧 Useful commands

```bash
cd gdsl && powershell -File test.ps1   # 编译 + 跑 35 用例（秒级）
```

---

## 📚 References (read IF needed)

- `.wolf/cerebrum.md` — User Preferences + Do-Not-Repeat + Decision Log
- `.wolf/anatomy.md` — token-efficient file index
- `.wolf/buglog.json` — known bugs + fixes
