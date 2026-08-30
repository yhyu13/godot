// gdsl/effect.h — effect/guard 解析：when/then 原文 → 结构化 AST（codegen 输入）
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §3.2（固定 effect ontology）、§7（ontology 边界）
#pragma once
#include <string>
#include <vector>

namespace gdsl {

// 字段引用所属参与者：self（规则所有者，= by 类型）或 target（碰撞对手方，= target 类型）。
enum class RefOwner { Self, Target };

// guard：<ref>.<field> <cmp> <literal>。v1 ontology 支持 self 与 target 引用（裸 field = self）。
struct Guard {
	RefOwner ref = RefOwner::Self; // 字段所属参与者
	std::string field; // 字段名，如 "hp"
	std::string cmp; // 比较符：> >= < <= == !=
	std::string value; // int/float 字面量，如 "0"、"0.5"
};

// effect 动词（ontology v1）：set_field 的三种赋值形式 + emit 信号发射（零参）。
enum class EffectKind { Set, Add, Sub, Emit };

struct Effect {
	EffectKind kind = EffectKind::Set;
	RefOwner ref = RefOwner::Self; // 字段所属参与者（set_field 用）
	std::string field; // 字段名（set_field 用）
	std::string value; // int/float 字面量（set_field 用）
	std::string signal_name; // 信号名（Emit 用）
};

// 解析 when 表达式（v1：单比较、self 引用、数值字面量）。成功 true，失败写 err。
bool parse_guard(const std::string &when, Guard &out, std::string &err);

// 解析 then 逗号分隔效果列表（v1：set_field + emit(<signal>)；emit 只支持零参）。
// 成功 true，失败写 err。
bool parse_effects(const std::string &then, std::vector<Effect> &out, std::string &err);

} // namespace gdsl
