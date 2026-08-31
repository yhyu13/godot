# Feature Specification: effect/guard 解析 — when/then 原文 → 结构化 AST

**Feature id**: `005-effect`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/effect.h/.cpp`（设计锚点 `GODOT_LLM_DSL_DESIGN.md` §3.2 固定 effect ontology、§7 ontology 边界）

## User Scenarios & Testing

### Primary User Story
作为逻辑层语义前端，我要把 rule 的 `when` 原文解析成结构化 `Guard`、把 `then`
原文解析成 `vector<Effect>`，遵循 ontology v1（单比较 + set_field/emit），
拒绝 ontology 之外的符号。

### Acceptance Scenarios
1. **Given** `self.hp > 0`，**When** 调用 `parse_guard`，**Then** 得到
   `field="hp"`、`cmp=">"`、`value="0"`、`ref=Self`。
2. **Given** `target.hp > 0`，**When** 调用 `parse_guard`，**Then** `ref=Target`。
3. **Given** `self.hp -= 1, emit(hit)`，**When** 调用 `parse_effects`，**Then**
   得到 2 个 effect：`Sub`（field=hp）与 `Emit`（signal=hit）。
4. **Given** `self.hp += 2` / `hp = 0` / `target.hp -= 1`，**When** 解析，**Then**
   分别得 `Add` / `Set`（bare field 视为 self）/ `Sub` 且 `ref=Target`。

### Edge Cases（均须拒绝并写 err）
- 未知 ref owner（`other.hp`）。
- 缺比较符、非数值 guard value。
- emit 带参数列表（v1 仅零参）、空信号名、未闭合括号。
- 缺赋值符的 effect。

## Requirements

### Functional Requirements
- **FR-001**: `parse_guard(when) -> Guard`，支持 self/target/bare 三种字段引用。
- **FR-002**: `parse_effects(then) -> vector<Effect>`，ontology v1 = Set/Add/Sub/Set + Emit。
- **FR-003**: ontology 之外的一切（未知 ref、非数值、带参 emit）拒绝并写 err。
- **FR-004**: `RefOwner::{Self,Target}` 与 `EffectKind::{Set,Add,Sub,Emit}` 为固定枚举，
  不改动时不静默扩展（morality：加 effect 先讨论）。

### Key Entities
- **Guard**: `ref` / `field` / `cmp` / `value`。
- **Effect**: `kind` / `ref` / `field` / `value` / `signal_name`。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏
- [x] 每个 FR 可测（ontology 拒绝是 falsifiable law）
- [x] 独立 seam 明确（`parse_guard` / `parse_effects`）
- [x] 测试存在于 `gdsl/test_effect.cpp`（17 条用例）
