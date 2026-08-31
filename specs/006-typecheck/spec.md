# Feature Specification: 逻辑层类型检查器 — AST → typed IR

**Feature id**: `006-typecheck`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/typecheck.h/.cpp`（设计锚点 `GODOT_LLM_DSL_DESIGN.md` §4/§5、`PLAN_LLM_DSL_IMPL.md` S4 — ptrcall 免 UB 的正确性闸门）

## User Scenarios & Testing

### Primary User Story
作为逻辑层类型系统，我要把 `Program` 类型检查成 `TypedProgram`（字段带 `ValueType`、
rule 带结构化 guard/effect），拒绝重复符号、未知类型/字段、跨参与者引用等错误，
因为 codegen 生成的 ptrcall 无类型校验，typecheck 是防 UB 的最后闸门。

### Acceptance Scenarios
1. **Given** 合法程序（Player + Bullet，含规则），**When** 调用 `typecheck`，
   **Then** 返回 true，字段类型解析为 `Int`/`Float`，rule 的 by/target 正确。
2. **Given** 字段类型引用已声明类型（`weapon: Weapon`），**When** typecheck，
   **Then** 得 `ValueType::Named` 且 `type_name="Weapon"`。
3. **Given** `rule ... when self.hp > 0 then self.hp -= 1`，**When** typecheck，
   **Then** 填结构化 `Guard{field=hp, cmp=">", value="0"}` 与
   `Effect{kind=Sub, field=hp, value="1"}`。
4. **Given** `emit(died)`，**When** typecheck，**Then** 得 `Emit{signal_name="died"}`。
5. **Given** `rule OnHit by Bullet target Player: when target.hp > 0 then target.hp -= 1`，
   **When** typecheck，**Then** guard.ref=Target、effect.ref=Target 且 by/target 均已知。

### Edge Cases（均须拒绝并写 err）
- 重复类型名 / 重复字段名。
- 未知字段类型（`hp: bogus`）。
- rule 触发类型未知（`by Ghost`）。
- rule 引用未知字段（`self.mana` / `target.mana`）。
- 无 target 子句却用 `target.*` 引用。
- target 类型未知（`target Ghost`）。

## Requirements

### Functional Requirements
- **FR-001**: `typecheck(Program) -> TypedProgram`，解析字段 `ValueType`（Int/Float/Bool/String/Named/Invalid）。
- **FR-002**: 填充结构化 guard/effect（复用 `005-effect` 的解析结果）。
- **FR-003**: 拒绝全部语义错误类别（重复/未知/跨参与者），写 err（law：可证伪）。
- **FR-004**: 校验 cross-participant 的 target 类型与字段必须已声明。

### Key Entities
- **TypedProgram / TypedType / TypedField / TypedRule**。
- **ValueType**: Int/Float/Bool/String/Named/Invalid。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏
- [x] 每个 FR 可测
- [x] 独立 seam 明确（`typecheck`）
- [x] 测试存在于 `gdsl/test_typecheck.cpp`（14 条用例，覆盖合法+6 类拒绝）
