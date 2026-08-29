// gdsl/codegen_logic.h — 逻辑层 codegen：typed IR → C（GDExtension）
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §4、PLAN §1 S5
#pragma once
#include <string>
#include "typecheck.h"

namespace gdsl {

// 生成 C 结构体定义 + 规则注释（确定性：同一输入逐字节同一输出）。
std::string emit_c(const TypedProgram &prog);

} // namespace gdsl
