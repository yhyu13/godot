// gdsl/scene_json.h — 声明式层：JSON 场景配方 → SceneSpec（schema 校验）
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §3.1、PLAN §1 S2
#pragma once
#include <string>
#include "codegen_declarative.h"

namespace gdsl {

// 解析并校验 JSON 场景配方；非法 JSON 或 schema 违反返回 false 并写 err。
bool scene_from_json(const std::string &json, SceneSpec &out, std::string &err);

} // namespace gdsl
