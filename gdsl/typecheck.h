// gdsl/typecheck.h — 逻辑层类型检查器：AST → typed IR（防止生成代码 UB 的闸门）
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §4、§5、PLAN §1 S4
#pragma once
#include <string>
#include <vector>
#include "parser.h"
#include "effect.h"

namespace gdsl {

enum class ValueType { Int, Float, Bool, String, Named, Invalid };

struct TypedField {
	std::string name;
	ValueType type = ValueType::Invalid;
	std::string type_name; // type == Named 时的类型名
	std::string default_value;
};

struct TypedType {
	std::string name;
	std::string base;
	std::vector<TypedField> fields;
	std::vector<std::string> signals; // 声明的信号名（FR-005 存在性校验词汇表）
};

struct TypedRule {
	std::string name;
	std::string by;
	std::string target; // 跨参与者 target 类型（空 = 无）
	std::string when;
	std::string then;
	Guard guard; // 结构化 when（ontology v1：self/target 字段比较）
	std::vector<Effect> effects; // 结构化 then（ontology v1：set_field / emit）
};

struct TypedProgram {
	std::vector<TypedType> types;
	std::vector<TypedRule> rules;
};

// 类型检查：拒绝重复类型名、未知字段类型、重复字段名、未知触发类型，
// 以及规则 when/then 不合 ontology（未知字段引用 / emit_signal / 跨参与者）。
// 成功返回 true 并填 typed IR；失败返回 false 并写 err。
bool typecheck(const Program &prog, TypedProgram &out, std::string &err);

} // namespace gdsl
