# Direction 5 计划 — String 字段 + StringName 泄漏 + manifest 一致性

> 目标：GDSL 热重载硬化第 2 波。上一波（方向 1→3）落地了标量 schema 容错 + Named 字段状态保持 + 转类型跳过。
> 本轮把「随路顺手做」的 ⑤（StringName 泄漏 + manifest 一致性）和 String 字段一起收尾。

## 前置调研（源码核实，非记忆）

- **ABI 无 `string_destroy`/`string_name_destroy`**：grep `core/extension/gdextension_interface.json` 的 `interface` 数组只见到 `variant_destroy`/`object_destroy`，无 string destroy。
- **variant 的 String 转换是 copy/placement-new 语义**（`core/variant/variant_internal.h:990-1006`）：`variant_from_type`=`memnew_placement(r, Variant(*((T*)p)))`（拷贝，源不释放）；`type_from_variant`=`memnew_placement(r, T(*VariantPtr))`（向 r placement-new，若 r 已构造则旧值泄漏）。
- ⇒ **任何 String 封送都会泄漏一个引用计数**，因为无 string_destroy 可释放。这是 ABI 硬墙（同 `variant_new_string_name`/`object_get_instance`），不可工程绕过，只能记录。
- **真 bug**：`name: string = "hero"` 发射 `self->name = ""hero"";`（双层引号 = C 语法错）。根因：parser 的 `parse_state_field` 把 `default_value` 存成原始 token `"hero"`（带引号），`default_literal` 的 String 分支又包了一层引号（`codegen_logic.cpp:35-43`）。任何带 String 默认值的 `.gdsl` 都会编译失败。
- **String 字段零参与热重载**：`variant_type(String)` 返回 nullptr（`codegen_logic.cpp:17-24`）⇒ getter/setter/property 循环 `continue`（`codegen_logic.cpp:328-331,503-505`）⇒ String 字段不注册属性、不保存状态，重载时静默丢到默认值。

## 决策（沿用项目 ABI 三墙模式：实现 + 记录限制）

- **D14**：String 字段走「保留状态 + 记录有界泄漏」。String 字段是合法类型，与 int/float/bool/Named 行为一致（保留），避免「标量都保、String 静默丢」的不一致。泄漏 = 每次热重载 getter/setter 各一个 transient GDExtensionString（开发期、几十字节），作为已知 ABI 限制记录在 cerebrum + JOURNEY，不新增「每发射一次建 StringName」类无界泄漏。
- **D15**：String 字段用干净 ownership：constructor 经 `gdsl_mem_alloc` 拷贝默认值（不用字符串字面量，避免 setter/free 撞字面量）；destructor `gdsl_mem_free`；setter 先 free 旧值再 alloc 新值。只有 transient getter/setter String 泄漏（ABI 墙），field buffer 本身不泄漏。

## 改动范围

- `gdsl/codegen_logic.cpp`：
  - `default_literal` String 分支：先剥离外层引号再包一层（修双层引号 bug）。
  - `variant_type` 加 String → `GDEXTENSION_VARIANT_TYPE_STRING`。
  - constructor/destructor 循环：String 字段特判（alloc 默认 / free）。
  - getter/setter/property 循环：String 字段特判分支（`string_new_with_utf8_chars` + `string_to_utf8_chars` 封送，schema-safe 类型检查：`variant_get_type != STRING → return`）。
  - API 缓存 + load_api：加 `string_new_with_utf8_chars`、`string_to_utf8_chars`。
- `gdsl/test_codegen.cpp`：加 String 字段用例（struct/constructor/双引号修复/getter-setter-property/类型检查 子串断言）。
- `gdsl/example/*.c`：用 `build_dll.ps1` 重生成。
- `gdsl/example/*.gdextension`：补/校对 manifest（reloadable=true、entry_symbol、dll 路径）。

## 不动

- `core/extension/gdextension.cpp`（另一 agent lane，segfault 引擎侧）。
- 退出 segfault（方向 1 已降级绕过：dev-only）。
- 规则层 String 字段（D13：string 不进规则层，维持）。
- Named 跨类型校验（已知限制）。

## 成功标准（本轮）

1. `string` 默认值生成合法 C（无双层引号）；`cl /LD` 语法绿。
2. String 字段生成 getter/setter/property，重载保留（生成代码层面验证）；schema-safe 类型检查在生成代码里（`variant_get_type != GDEXTENSION_VARIANT_TYPE_STRING → return`）。
3. StringName 泄漏：信号名 StringName 是 `p_is_static=true` 且单次创建（测试断言）；无 per-emit 创建；记录无 destroy ABI 限制。
4. manifest 一致性：`.gdextension` 声明 `reloadable=true`（热重载 recreate 路径前提）+ `entry_symbol=gdsl_library_init` + dll 路径对齐。
5. 无回归：`gdsl/test.ps1` 全绿（89/353 → +String 用例）。
