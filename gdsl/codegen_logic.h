// gdsl/codegen_logic.h — 逻辑层 codegen：typed IR → C（GDExtension）
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §4、PLAN §1 S5
#pragma once
#include <string>
#include "typecheck.h"

namespace gdsl {

// 生成完整 GDExtension C 源（确定性：同一输入逐字节同一输出）：
// 入口点 gdsl_library_init + SCENE 级 initialize/deinitialize + 每类型
// classdb_register_extension_class6 注册 + create/free 实例生命周期 + 状态字段
// 落进 C struct + 规则编译成方法（guard + set_field，经
// classdb_register_extension_class_method 注册）。
std::string emit_c(const TypedProgram &prog);

} // namespace gdsl
