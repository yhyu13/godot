# Feature Specification: 逻辑层解析器 — .gdsl → AST

**Feature id**: `004-parser`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/parser.h/.cpp`（设计锚点 `GODOT_LLM_DSL_DESIGN.md` §3.2：小语法 + 强类型 + 固定 effect ontology）

## User Scenarios & Testing

### Primary User Story
作为逻辑层前端，我要把 `.gdsl` 源文本（`type` 块 + `rule` 块）解析成 `Program`
AST，拒绝语法不合法的输入并给出非空 `err`，供 typechecker 消费。

### Acceptance Scenarios
1. **Given** `type Player @extends CharacterBody2D`，**When** 调用
   `parse_type_decl`，**Then** 得到 `name="Player"`、`base="CharacterBody2D"`。
2. **Given** `hp: int = 3`，**When** 调用 `parse_state_field`，**Then** 得到
   `name="hp"`、`type="int"`、`default_value="3"`。
3. **Given** 一个多行 type 块（含 `state:` 字段列表），**When** 调用
   `parse_type_block`，**Then** 字段列表按序完整。
4. **Given** `rule OnHit by Bullet target Player:` + `when`/`then` 行，**When**
   调用 `parse_rule_block`，**Then** 得到 name/by/target/when/then 原文。
5. **Given** 完整程序（type + rule），**When** 调用 `parse_program`，**Then**
   得到含 1 type + 1 rule 的 `Program`。

### Edge Cases（均须拒绝并写 err）
- 缺 `@extends`、缺 base、缺冒号、缺类型、缺默认值。
- rule 缺 `when` / 缺 `then`。
- rule 头部出现未知 token（如 `bogus`）。
- `target:` 子句缺类型名。
- 程序中出现无法识别的行。

## Requirements

### Functional Requirements
- **FR-001**: 解析 type 声明与 state 字段（含 int/float 类型、默认值）。
- **FR-002**: 解析 rule 块（by / 可选 target / when / then 原文透传）。
- **FR-003**: 解析整个程序为 `Program`（types + rules）。
- **FR-004**: 所有非法输入返回 false 且 `err` 非空（law：可证伪）。

### Key Entities
- **Program**: `vector<TypeDecl> types` + `vector<RuleDecl> rules`。
- **TypeDecl**: `name` / `base` / `fields`。
- **FieldDecl**: `name` / `type` / `default_value`。
- **RuleDecl**: `name` / `by` / `target`（可空）/ `when` / `then`（原文）。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏
- [x] 每个 FR 可测（非法输入拒绝是 falsifiable law）
- [x] 独立 seam 明确（`parse_*` 五函数）
- [x] 测试存在于 `gdsl/test_parser.cpp`（18 条用例）
