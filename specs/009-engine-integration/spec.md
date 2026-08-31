# Feature Specification: 引擎集成 — .gdextension 加载真实场景（S6）

**Feature id**: `009-engine-integration`
**Created**: 2026-08-30
**Status**: Draft（forward-spec — 待实现）
**Input**: `GODOT_LLM_DSL_DESIGN.md` §5「端到端」行 + `PLAN_LLM_DSL_IMPL.md` S6

## User Scenarios & Testing

### Primary User Story
作为 DSL 编译器用户，我要让一份声明式 JSON 场景配方 + 逻辑 `.gdsl` 规则编译后，
被真实 Godot 引擎加载并实例化出一个节点树/连接数与声明一致的场景，
从而证明两层 DSL 管线端到端可用（不只是能编译）。

### Acceptance Scenarios（falsifiable bar）
1. **Given** gdsl 生成的 C 源（`008-codegen-logic` 输出），**When** 用 Godot
   GDExtension 工具链编译为动态库并配 `.gdextension` 清单，**Then** 编译 exit 0
   且库文件生成。
2. **Given** 编译出的动态库 + 一个由 `003-codegen-declarative` 生成的 `.tscn`，
   **When** 由真实 Godot editor/template 二进制加载，**Then** 引擎日志无
   `ResourceFormatLoaderText` 解析错误、无 GDExtension 加载错误。
3. **Given** 上述加载，**When** 用 `[SceneTree]` doctest 断言实例化结果，**Then**
   节点树节点数与连接数 == 声明式 JSON 配方中的数量（多一个/少一个 = 红）。
4. **Given** 逻辑规则 `OnHit`（`by Bullet target Player`），**When** 在实例化场景
   中触发，**Then** target 字段按 effect 更新（`target.hp -= 1`），经
   `object_method_bind_ptrcall` 直连 C++ 生效。

### Edge Cases
- 空场景（仅 root）加载不崩溃。
- 引擎找不到 `.gdextension` 清单时的错误路径。
- 生成代码与引擎 ABI 版本不匹配（哈希漂移，见 `007-abihash` 黄金值设计意图）。

## Requirements

### Functional Requirements
- **FR-001**: 提供编译管线：`emit_c` 输出 → 动态库 → `.gdextension` 清单。
- **FR-002**: 真实引擎加载生成的 `.tscn`，无解析错误。
- **FR-003**: `[SceneTree]` doctest 断言节点树/连接数与声明一致（falsifiable law）。
- **FR-004**: 逻辑规则经 ptrcall 生效，field 更新可观测。
- **FR-005**: 集成测试落在 `tests/scene/`（SOP §2.1 分层约定），tag `[SceneTree]`。

### Key Entities
- **.gdextension 清单**、**动态库**、**生成 .tscn**。

## Review & Acceptance Checklist
- [ ] 无实现细节泄漏
- [ ] 每个 FR 可测（FR-003 是 falsifiable law）
- [ ] 独立 seam 明确（加载 + `[SceneTree]` 断言）
- [ ] 引擎集成测试落地（尚未实现，本 spec 为 forward 起点）
