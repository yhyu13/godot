#include "typecheck.h"
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

static bool has_field(const TypedType &t, const std::string &name) {
	for (const TypedField &f : t.fields) {
		if (f.name == name) {
			return true;
		}
	}
	return false;
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

	// Pass 2：检查每个类型的字段
	for (const TypeDecl &t : prog.types) {
		TypedType tt;
		tt.name = t.name;
		tt.base = t.base;
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
			if (ref_type != nullptr && !has_field(*ref_type, tr.guard.field)) {
				err = "rule '" + r.name + "': unknown field '" + tr.guard.field + "' in type '" + ref_type->name + "'";
				return false;
			}
		}
		if (!parse_effects(r.then, tr.effects, err)) {
			err = "rule '" + r.name + "': " + err;
			return false;
		}
		for (const Effect &fx : tr.effects) {
			if (fx.kind == EffectKind::Emit) {
				// 信号发射：不引用字段，信号名已在 parse_effects 校验为合法标识符。
				continue;
			}
			const TypedType *ref_type = (fx.ref == RefOwner::Target) ? target : owner;
			if (fx.ref == RefOwner::Target && ref_type == nullptr) {
				err = "rule '" + r.name + "': 'target' referenced but no 'target <Type>' clause";
				return false;
			}
			if (ref_type != nullptr && !has_field(*ref_type, fx.field)) {
				err = "rule '" + r.name + "': unknown field '" + fx.field + "' in type '" + ref_type->name + "'";
				return false;
			}
		}
		out.rules.push_back(std::move(tr));
	}

	return true;
}

} // namespace gdsl
