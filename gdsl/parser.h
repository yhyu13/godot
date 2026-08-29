// gdsl/parser.h — LLM DSL 逻辑层解析器（独立内核，不依赖 Godot core）
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §3.2（小语法 + 强类型 + 固定 effect ontology）
#pragma once
#include <string>

namespace gdsl {

struct TypeDecl {
	std::string name; // 类型名，如 "Player"
	std::string base; // 基类，如 "CharacterBody2D"
};

struct FieldDecl {
	std::string name; // 字段名，如 "hp"
	std::string type; // 类型，如 "int"、"float"
	std::string default_value; // 默认值原文，如 "3"、"400.0"
};

// 解析单行 type 声明 "type Name @extends Base"。
// 成功返回 true 并填 out；失败返回 false 并在 err 写原因。
bool parse_type_decl(const std::string &line, TypeDecl &out, std::string &err);

// 解析单行 state 字段 "name: type = default"。
bool parse_state_field(const std::string &line, FieldDecl &out, std::string &err);

} // namespace gdsl
