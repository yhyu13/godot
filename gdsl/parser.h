// gdsl/parser.h — LLM DSL 逻辑层解析器（独立内核，不依赖 Godot core）
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §3.2（小语法 + 强类型 + 固定 effect ontology）
#pragma once
#include <string>
#include <vector>

namespace gdsl {

struct FieldDecl {
	std::string name; // 字段名，如 "hp"
	std::string type; // 类型，如 "int"、"float"
	std::string default_value; // 默认值原文，如 "3"、"400.0"
};

struct TypeDecl {
	std::string name; // 类型名，如 "Player"
	std::string base; // 基类，如 "CharacterBody2D"
	std::vector<FieldDecl> fields; // state: 块里的字段列表
};

struct RuleDecl {
	std::string name; // 规则名，如 "OnHit"
	std::string by; // 触发源类型，如 "Bullet"
	std::string when; // guard 表达式原文，如 "target.hp > 0"
	std::string then; // effect 列表原文，如 "target.hp -= 1, emit(hit)"
};

struct Program {
	std::vector<TypeDecl> types; // 类型块列表
	std::vector<RuleDecl> rules; // 规则块列表
};

// 解析单行 type 声明 "type Name @extends Base"。
bool parse_type_decl(const std::string &line, TypeDecl &out, std::string &err);

// 解析单行 state 字段 "name: type = default"。
bool parse_state_field(const std::string &line, FieldDecl &out, std::string &err);

// 解析多行 type 块："type Name @extends Base" + 可选 "state:" 字段列表。
bool parse_type_block(const std::string &text, TypeDecl &out, std::string &err);

// 解析多行 rule 块："rule Name by Source:" + "when guard" + "then effects"。
bool parse_rule_block(const std::string &text, RuleDecl &out, std::string &err);

// 解析整个 .gdsl 源文本（多个 type/rule 块）→ Program AST。
bool parse_program(const std::string &source, Program &out, std::string &err);

} // namespace gdsl
