# Feature Specification: 最小 JSON 解析器

**Feature id**: `001-json`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/json.h` / `gdsl/json.cpp`（独立内核，无第三方依赖）

## User Scenarios & Testing

### Primary User Story
作为 DSL 编译器内核，我要把一段 JSON 文本解析成 `JsonValue` 树，并按键取值，
这样声明式层（`scene_json`）才能在无需第三方库的情况下读取场景配方。

### Acceptance Scenarios
1. **Given** 一段含 object/array/string/number/bool/null 的合法 JSON 文本，
   **When** 调用 `parse_json`，**Then** 返回 true 且 `JsonValue` 树结构与文本一致。
2. **Given** 一段非法 JSON（如缺右括号、语法错误），**When** 调用 `parse_json`，
   **Then** 返回 false 且 `err` 非空。
3. **Given** 一个 object 类型的 `JsonValue`，**When** 用不存在的 key 调用 `find`，
   **Then** 返回 `nullptr`（不越界、不崩溃）。

### Edge Cases
- 空对象 `{}`、空数组 `[]`。
- 数值的负数与浮点（`-1`、`0.5`）。
- 字符串转义（`\"`、`\\`）。
- 嵌套 object/array 的递归深度。

> 注意（诚实记录）：`json` 目前**没有**独立测试文件（`gdsl/` 下无 `test_json.cpp`），
> 只通过 `scene_json` 的测试（`test_declarative.cpp`）间接覆盖。这是一个
> 覆盖率缺口，见 FR-003。

## Requirements

### Functional Requirements
- **FR-001**: 解析 JSON 标准子集 object/array/string/number/bool/null，失败写 `err`。
- **FR-002**: `JsonValue::find(key)` 仅对 object 生效；key 不存在或非 object 返回 `nullptr`。
- **FR-003**: 为 `parse_json` 补独立单测（非法文本、空容器、转义、嵌套），覆盖
  直接 seam，消除「仅间接覆盖」的缺口。

### Key Entities
- **JsonValue**: `Type`（Null/Bool/Number/String/Array/Object）+ 各类型取值字段 +
  `object` 为 `vector<pair<string,JsonValue>>`。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏（不描述 Godot core 内部）
- [x] 每个 FR 可测
- [x] 独立 seam 已明确（`parse_json` / `find`）
- [ ] FR-003（补直接单测）尚未落地，属待办而非本 retro-spec 范围
