# Feature Specification: 声明式场景配方解析与 schema 校验

**Feature id**: `002-scene-json`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/scene_json.h` / `gdsl/scene_json.cpp`（设计锚点 `GODOT_LLM_DSL_DESIGN.md` §3.1、`PLAN_LLM_DSL_IMPL.md` S2）

## User Scenarios & Testing

### Primary User Story
作为声明式层入口，我要把一份 JSON 场景配方（root + children + props）解析并
校验成 `SceneSpec`，拒绝缺失必填字段或类型错误的配方，供 codegen 生成 `.tscn`。

### Acceptance Scenarios
1. **Given** 一份含 `root`、`children`、`props.position`（`{x,y}` 对象）的合法
   JSON 配方，**When** 调用 `scene_from_json`，**Then** 返回 true 且 `SceneSpec`
   的节点树、属性名、序列化值（`Vector2(10, 20)`）与声明一致。
   （对应 `test_declarative.cpp` "Parse a scene JSON recipe"）
2. **Given** 语法损坏的 JSON，**When** 调用 `scene_from_json`，**Then** 返回
   false 且 `err` 非空。
3. **Given** 缺 `root` 的 JSON，**When** 调用 `scene_from_json`，**Then** 返回
   false 且 `err` 非空。
4. **Given** root 节点缺 `type` 的 JSON，**When** 调用 `scene_from_json`，
   **Then** 返回 false 且 `err` 非空。

### Edge Cases
- `position` 的 `{x,y}` 对象须被序列化为 `Vector2(x, y)` 字面量。
- 未知的顶层键（除 root/children/props 外的字段）。
- 空 `children` 数组（合法，生成仅 root 的场景）。

## Requirements

### Functional Requirements
- **FR-001**: 解析 JSON 场景配方为 `SceneSpec`（复用 `001-json`）。
- **FR-002**: 必填校验：root 必须存在、每个节点必须有 `type`；违反写 `err` 并拒绝。
- **FR-003**: `props` 中的 Godot 复合字面量（如 `Vector2`）按 `{x,y}` 结构化输入
  序列化为引擎字面量文本。
- **FR-004**: 确定性：同一输入产出的 `SceneSpec` 一致。

### Key Entities
- **SceneSpec / NodeSpec / Prop**（定义见 `003-codegen-declarative`）。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏
- [x] 每个 FR 可测
- [x] 独立 seam 明确（`scene_from_json`）
- [x] 测试已存在于 `gdsl/test_declarative.cpp`（含 4 条验收场景）
