# Feature Specification: 声明式 codegen — SceneSpec → .tscn

**Feature id**: `003-codegen-declarative`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/codegen_declarative.h/.cpp`（设计锚点 `GODOT_LLM_DSL_DESIGN.md` §3.1、`PLAN_LLM_DSL_IMPL.md` S1 tracer bullet）

## User Scenarios & Testing

### Primary User Story
作为声明式层后端，我要把 `SceneSpec` 编译成 Godot 可加载的 `.tscn` 文本，
输出确定（同一输入逐字节同一输出），交给引擎的 `ResourceFormatLoaderText`
直接解析。

### Acceptance Scenarios
1. **Given** 一个含 root（Node2D）与子节点（Sprite2D，带 position 属性）的
   `SceneSpec`，**When** 调用 `emit_tscn`，**Then** 输出含 `[gd_scene format=3]`、
   `[node name="Main" type="Node2D"]`、子节点 `parent="."` 与 `position = Vector2(10, 20)`。
2. **Given** 同一 `SceneSpec`，**When** 调用 `emit_tscn` 两次，**Then** 逐字节一致
   （golden 精确匹配，含换行与缩进）。
3. **Given** 一个 JSON 配方，**When** 先 `scene_from_json` 再 `emit_tscn`，
   **Then** 端到端产出可预期的 `.tscn` 文本（tracer bullet）。

### Edge Cases
- 无属性的节点（不输出多余空行）。
- 多层嵌套子节点（递归缩进正确）。
- 根节点无父引用（不输出 `parent` 属性）。

## Requirements

### Functional Requirements
- **FR-001**: `emit_tscn(SceneSpec) -> String` 输出 `[gd_scene format=3]` 头。
- **FR-002**: 节点块 `[node name="..." type="..."]`，非 root 节点带 `parent="."`。
- **FR-003**: 属性以 `name = value` 行输出，value 为已序列化的 Godot 字面量。
- **FR-004**: 确定性（law：同一输入逐字节同一输出），golden 测试锁定格式。

### Key Entities
- **SceneSpec**: `NodeSpec root`（整棵树挂 root.children）。
- **NodeSpec**: `type` / `name` / `props` / `children`。
- **Prop**: `name` / `value`（已序列化字面量）。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏
- [x] 每个 FR 可测（确定性是 property-based 的 law）
- [x] 独立 seam 明确（`emit_tscn`）
- [x] 测试存在于 `gdsl/test_declarative.cpp`（minimal / golden / deterministic / e2e）
