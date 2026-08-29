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

	// Pass 3：检查规则引用的触发类型
	for (const RuleDecl &r : prog.rules) {
		if (!is_declared(r.by, type_names)) {
			err = "unknown trigger type '" + r.by + "' in rule '" + r.name + "'";
			return false;
		}
		TypedRule tr;
		tr.name = r.name;
		tr.by = r.by;
		tr.when = r.when;
		tr.then = r.then;
		out.rules.push_back(std::move(tr));
	}

	return true;
}

} // namespace gdsl
