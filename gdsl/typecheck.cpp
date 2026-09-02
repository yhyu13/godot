#include "typecheck.h"
#include "godot_classes.h"
#include <cerrno>
#include <cstdlib>
#include <utility>

namespace gdsl {

static ValueType resolve_type(const std::string &name, const std::vector<std::string> &type_names, std::string &type_name) {
	if (name == "int") {
		return ValueType::Int;
	}
	if (name == "float") {
		return ValueType::Float;
	}
	if (name == "bool") {
		return ValueType::Bool;
	}
	if (name == "string") {
		return ValueType::String;
	}
	for (const std::string &n : type_names) {
		if (n == name) {
			type_name = name;
			return ValueType::Named;
		}
	}
	return ValueType::Invalid;
}

static bool is_declared(const std::string &name, const std::vector<std::string> &type_names) {
	for (const std::string &n : type_names) {
		if (n == name) {
			return true;
		}
	}
	return false;
}

static const TypedType *find_type(const std::vector<TypedType> &types, const std::string &name) {
	for (const TypedType &t : types) {
		if (t.name == name) {
			return &t;
		}
	}
	return nullptr;
}

static const TypedField *find_field(const TypedType &t, const std::string &name) {
	for (const TypedField &f : t.fields) {
		if (f.name == name) {
			return &f;
		}
	}
	return nullptr;
}

// 字面量分类（数值语法与 effect.cpp is_number_literal 一致：可选负号 + 数字 + 可选小数点数字）。
enum class LitKind { Int, Float, Bool, Null, Other };

static LitKind classify_literal(const std::string &s) {
	if (s == "true" || s == "false") {
		return LitKind::Bool;
	}
	if (s == "null") {
		return LitKind::Null;
	}
	size_t i = 0;
	if (!s.empty() && s[0] == '-') {
		i = 1;
	}
	if (i >= s.size()) {
		return LitKind::Other;
	}
	size_t digits = 0;
	bool dot = false;
	for (; i < s.size(); i++) {
		const char c = s[i];
		if (c >= '0' && c <= '9') {
			digits++;
		} else if (c == '.' && !dot) {
			dot = true;
		} else {
			return LitKind::Other;
		}
	}
	if (digits == 0) {
		return LitKind::Other;
	}
	return dot ? LitKind::Float : LitKind::Int;
}

// 字面量是否兼容字段类型。v1 政策：无隐式转换，仅 int→float 拓宽；Named 仅 'null'。
static bool int_literal_fits_int64(const std::string &s) {
	errno = 0;
	char *end = nullptr;
	std::strtoll(s.c_str(), &end, 10);
	if (end == s.c_str() || errno == ERANGE) {
		return false;
	}
	return true;
}

// allow_int_float_widen: 是否允许 int 字面量拓宽到 float 字段。
// 赋值/默认值允许（5 存入 float 字段语义精确 5.0）；但 guard 比较必须类型精确——`when self.spd == 5`
// 会把 int 隐式拓宽成 float 再比较，浮点值对整型字面量做 == 是静默精度语义风险（LLM_UNFRIENDLY #4
// 「无隐式转换」政策）。比较路径传 false 强制写 5.0。
static bool literal_matches(ValueType field_type, const std::string &literal, std::string &why, bool allow_int_float_widen = true) {
	const LitKind k = classify_literal(literal);
	switch (field_type) {
		case ValueType::Int:
			if (k == LitKind::Int) {
				// FR-005 (int_overflow): int 字段在 codegen 里以 int64 存储；超出 int64 的字面量
				// 会被原样发射成 C 整型字面量，溢出后存储值错误（wraps）。在此提前拒收。
				if (!int_literal_fits_int64(literal)) {
					why = "integer literal out of int64 range: '" + literal + "'";
					return false;
				}
				return true;
			}
			why = "expected integer literal, got '" + literal + "'";
			return false;
		case ValueType::Float:
			if (k == LitKind::Float) {
				return true;
			}
			if (k == LitKind::Int && allow_int_float_widen) {
				return true;
			}
			why = "expected float literal for float field, got '" + literal + "' (write it as a float, e.g. 5.0)";
			return false;
		case ValueType::Bool:
			if (k == LitKind::Bool) {
				return true;
			}
			why = "expected bool literal, got '" + literal + "'";
			return false;
		case ValueType::String:
			// FR-005: string 字段默认值必须是双引号字符串字面量；裸数字/标识符会被 codegen
			// 当成字段赋值（`self->name = 123;` 对 char* 是错/失效代码），故在此提前拒收。
			if (k == LitKind::Other && literal.size() >= 2 && literal.front() == '"' && literal.back() == '"') {
				return true;
			}
			why = "expected a double-quoted string literal, got '" + literal + "'";
			return false;
		case ValueType::Named:
			if (k == LitKind::Null) {
				return true;
			}
			why = "expected 'null', got '" + literal + "'";
			return false;
		default:
			why = "invalid field type";
			return false;
	}
}

bool typecheck(const Program &prog, TypedProgram &out, std::string &err) {
	out.types.clear();
	out.rules.clear();

	// Pass 1：收集类型名并检测重复
	std::vector<std::string> type_names;
	for (const TypeDecl &t : prog.types) {
		if (is_declared(t.name, type_names)) {
			err = "duplicate type '" + t.name + "'";
			return false;
		}
		type_names.push_back(t.name);
	}

	// Pass 2：检查每个类型的字段 + @extends 基类名是否真实 Godot 类（Gap B / ClassDB 校验）
	for (const TypeDecl &t : prog.types) {
		// @extends 必须是 extension_api.json 的 classes 表里真实存在的类，否则生成 C 后
		// classdb_construct_object(base) 在运行时才崩。这里把「幻觉→运行时崩」提前成「编译错→改一下」。
		if (!is_godot_class(t.base)) {
			err = "type '" + t.name + "': @extends '" + t.base + "' is not a real Godot class";
			return false;
		}
		TypedType tt;
		tt.name = t.name;
		tt.base = t.base;
		tt.signals = t.signals;
		for (const FieldDecl &f : t.fields) {
			for (const TypedField &existing : tt.fields) {
				if (existing.name == f.name) {
					err = "duplicate field '" + f.name + "' in type '" + t.name + "'";
					return false;
				}
			}
			TypedField tf;
			tf.name = f.name;
			tf.default_value = f.default_value;
			tf.type = resolve_type(f.type, type_names, tf.type_name);
			if (tf.type == ValueType::Invalid) {
				err = "unknown field type '" + f.type + "' in field '" + f.name + "'";
				return false;
			}
			std::string why;
			if (!literal_matches(tf.type, tf.default_value, why)) {
				err = "type '" + t.name + "' field '" + f.name + "': " + why;
				return false;
			}
			tt.fields.push_back(std::move(tf));
		}
		out.types.push_back(std::move(tt));
	}

	// Pass 3：检查规则引用的触发类型 + when/then 合 ontology
	for (const RuleDecl &r : prog.rules) {
		if (!is_declared(r.by, type_names)) {
			err = "unknown trigger type '" + r.by + "' in rule '" + r.name + "'";
			return false;
		}
		// FR-005 (dup_rule_name): 同一 owner 类型上两个同名 rule 会在 codegen 里注册两个同名方法，
		// 属重复注册（哪个生效不确定 / 引擎报错）。在此对 (name, by) 重复拒收。
		for (const TypedRule &er : out.rules) {
			if (er.name == r.name && er.by == r.by) {
				err = "duplicate rule '" + r.name + "' for type '" + r.by + "'";
				return false;
			}
		}
		const TypedType *owner = find_type(out.types, r.by);
		const TypedType *target = nullptr;
		if (!r.target.empty()) {
			if (!is_declared(r.target, type_names)) {
				err = "rule '" + r.name + "': unknown target type '" + r.target + "'";
				return false;
			}
			target = find_type(out.types, r.target);
		}
		TypedRule tr;
		tr.name = r.name;
		tr.by = r.by;
		tr.target = r.target;
		tr.when = r.when;
		tr.then = r.then;
		if (!parse_guard(r.when, tr.guard, err)) {
			err = "rule '" + r.name + "': " + err;
			return false;
		}
		// guard 字段校验：按 ref 选 owner（self → by 类型，target → target 类型）。
		{
			const TypedType *ref_type = (tr.guard.ref == RefOwner::Target) ? target : owner;
			if (tr.guard.ref == RefOwner::Target && ref_type == nullptr) {
				err = "rule '" + r.name + "': 'target' referenced but no 'target <Type>' clause";
				return false;
			}
			const TypedField *gf = (ref_type != nullptr) ? find_field(*ref_type, tr.guard.field) : nullptr;
			if (gf == nullptr) {
				err = "rule '" + r.name + "': unknown field '" + tr.guard.field + "' in type '" + (ref_type ? ref_type->name : "?") + "'";
				return false;
			}
			std::string why;
			if (!literal_matches(gf->type, tr.guard.value, why, /*allow_int_float_widen=*/false)) {
				err = "rule '" + r.name + "': " + why + " (guard '" + tr.guard.field + "')";
				return false;
			}
		}
		if (!parse_effects(r.then, tr.effects, err)) {
			err = "rule '" + r.name + "': " + err;
			return false;
		}
		for (const Effect &fx : tr.effects) {
			if (fx.kind == EffectKind::Emit) {
				// FR-005: 信号存在性校验。codegen 会把每个 emit(<sig>) 在 rule 的 owner(by) 类型上注册
				// 为信号；若信号名未在该类型用 signals: 声明，则要么是拼错（如 hitt 当 hit），要么是
				// 随意构造——连接方 connect 该名会静默失败（LLM_UNFRIENDLY #4 的「待钉死」）。
				// 需要在 type 上声明，提前把「静默 no-op」变成「编译错→改一下」。
				const TypedType *sig_owner = owner;
				bool declared = false;
				if (sig_owner != nullptr) {
					for (const std::string &s : sig_owner->signals) {
						if (s == fx.signal_name) {
							declared = true;
							break;
						}
					}
				}
				if (!declared) {
					err = "rule '" + r.name + "': signal '" + fx.signal_name + "' is not declared in type '" +
							(sig_owner ? sig_owner->name : "?") + "' (add 'signals: " + fx.signal_name + "' to the type)";
					return false;
				}
				continue;
			}
			const TypedType *ref_type = (fx.ref == RefOwner::Target) ? target : owner;
			if (fx.ref == RefOwner::Target && ref_type == nullptr) {
				err = "rule '" + r.name + "': 'target' referenced but no 'target <Type>' clause";
				return false;
			}
			const TypedField *ff = (ref_type != nullptr) ? find_field(*ref_type, fx.field) : nullptr;
			if (ff == nullptr) {
				err = "rule '" + r.name + "': unknown field '" + fx.field + "' in type '" + (ref_type ? ref_type->name : "?") + "'";
				return false;
			}
			std::string why;
			if (!literal_matches(ff->type, fx.value, why)) {
				err = "rule '" + r.name + "': " + why + " (effect '" + fx.field + "')";
				return false;
			}
		}
		out.rules.push_back(std::move(tr));
	}

	return true;
}

} // namespace gdsl
