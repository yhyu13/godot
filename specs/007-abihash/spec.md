# Feature Specification: GDExtension method-bind 兼容哈希（abihash）

**Feature id**: `007-abihash`
**Created**: 2026-08-30
**Status**: Complete（retro-spec — 记录已实现组件）
**Input**: `gdsl/abihash.h`（逐字移植 `core/object/method_info.cpp:87-116` + `hashfuncs.h` murmur3）

## User Scenarios & Testing

### Primary User Story
作为逻辑层 codegen 的支撑，我要计算 `Object::emit_signal` 的 GDExtension
兼容哈希（MethodBind compatibility hash），使生成的 C 能用
`classdb_get_method_bind` 精确定位 emit_signal 的方法绑定。

### Acceptance Scenarios
1. **Given** 空调用，**When** 调用 `emit_signal_compat_hash()`，**Then** 等于
   黄金值 `2866548813`（`0xAADC104D`），由独立 standalone 程序按 Godot 源码算法
   预计算（`doc_ai/PLAN_LLM_DSL_IMPL.md` D5）。
2. **Given** 任意输入，**When** 调用 `hash_fmix32(0)`，**Then** 等于 0（可手推）。
3. **Given** 同一输入，**When** 两次调用 `hash_murmur3_one_32`，**Then** 结果一致（确定性）。

### Edge Cases
- 若引擎侧 `MethodInfo` 结构改变导致哈希漂移，黄金值测试应红（提醒重核
  `extension_api.json` dump）——这是本组件的设计意图，非 bug。

## Requirements

### Functional Requirements
- **FR-001**: 逐字移植 murmur3 `hash_murmur3_one_32`（seed=`HASH_MURMUR3_SEED=0x7F07C65`）。
- **FR-002**: 逐字移植 `hash_fmix32`。
- **FR-003**: `emit_signal_compat_hash()` 按 `MethodInfo::get_compatibility_hash`
  流程重建（无返回 + 1 个 STRING_NAME 参数 + 无默认 + vararg），锚定黄金值。

### Key Entities
- 纯函数，无状态实体。

## Review & Acceptance Checklist
- [x] 无实现细节泄漏（本组件本身就是对引擎算法的精确移植，锚定源码行）
- [x] 每个 FR 可测（黄金值是 falsifiable law）
- [x] 独立 seam 明确（`emit_signal_compat_hash` / `hash_*`）
- [x] 测试存在于 `gdsl/test_abihash.cpp`（黄金值 + 确定性）
