# Feature Specification: 逻辑层 codegen — typed IR → C (GDExtension)

**Feature id**: `008-codegen-logic`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/codegen_logic.h/.cpp`（设计锚点 `GODOT_LLM_DSL_DESIGN.md` §4、`PLAN_LLM_DSL_IMPL.md` S5）

## User Scenarios & Testing

### Primary User Story
作为逻辑层后端，我要把 `TypedProgram` 编译成完整可编译的 GDExtension C 源
（库入口 + 每类型 classdb 注册 + 实例生命周期 + 状态字段 C struct + 规则编译为
方法），输出确定，交给引擎 GDExtension 工具链编译成动态库。

### Acceptance Scenarios
1. **Given** 一个类型，**When** 调用 `emit_c`，**Then** 输出含
   `#include <gdextension_interface.h>`、`gdsl_library_init` 入口、
   `minimum_initialization_level = SCENE`。
2. **Given** 类型 `Player @extends CharacterBody2D`，**When** `emit_c`，**Then**
   含 `classdb_register_extension_class6` + create/free 实例生命周期
   （`gdsl_classdb_construct_object` / `gdsl_mem_alloc(sizeof(Player))`）。
3. **Given** 带 state 字段的类型，**When** `emit_c`，**Then** 生成 C struct
   `int64_t hp;` / `double speed;` 与默认值赋值；Named 字段为指针 + NULL。
4. **Given** 规则 `TakeDamage`（guard + set_field），**When** `emit_c`，**Then**
   生成方法 `player_take_damage_impl`（guard 变 `if (!(...))`、effect 变
   `self->hp -= 1;`）并注册 `call_func`/`ptrcall_func`。
5. **Given** `emit(died)` 规则，**When** `emit_c`，**Then** 缓存
   `classdb_get_method_bind`（精确哈希 `2866548813`）+ StringName 缓存 +
   `object_method_bind_call` 封送；重复信号名只声明一次。
6. **Given** cross-participant 规则（`by Bullet target Player`），**When**
   `emit_c`，**Then** 方法带 `GDExtensionObjectPtr p_target` 参数，经
   `object_get_instance_binding` 取 target struct，`argument_count=1` + OBJECT 参数信息。
7. **Given** 任意输入，**When** 两次 `emit_c`，**Then** 逐字节一致（确定性）。

### Edge Cases
- 无规则的纯类型：不注册任何方法。
- `+=`/`=`/`-=` 映射到正确 C 运算符。

## Requirements

### Functional Requirements
- **FR-001**: `emit_c(TypedProgram) -> String` 生成完整 GDExtension 入口 + SCENE 初始化级别。
- **FR-002**: 每类型 classdb 注册 + 实例生命周期 + 状态 C struct。
- **FR-003**: 规则 → 方法（guard 反转为 early-return + effect → C 语句）。
- **FR-004**: emit 信号经 method-bind 精确哈希 + Variant 封送 + 信号名去重。
- **FR-005**: cross-participant 经 instance binding + OBJECT 参数。
- **FR-006**: 确定性（law：同一输入逐字节同一输出）。

### Key Entities
- 输出为 C 源文本（`TypedProgram` 输入，无中间实体）。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏
- [x] 每个 FR 可测（确定性是 law）
- [x] 独立 seam 明确（`emit_c`）
- [x] 测试存在于 `gdsl/test_codegen.cpp`（14 条用例，覆盖入口/注册/生命周期/规则/emit/cross/确定性）
