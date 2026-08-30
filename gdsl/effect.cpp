#include "effect.h"

namespace gdsl {

static std::string trim(const std::string &s) {
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) {
		return "";
	}
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

static bool is_ident(const std::string &s) {
	if (s.empty()) {
		return false;
	}
	char c0 = s[0];
	if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= 'A' && c0 <= 'Z') || c0 == '_')) {
		return false;
	}
	for (size_t i = 1; i < s.size(); i++) {
		char c = s[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
			return false;
		}
	}
	return true;
}

// 数值字面量：可选负号 + 数字 + 可选小数点数字。v1 不支持 hex/科学计数。
static bool is_number_literal(const std::string &s) {
	if (s.empty()) {
		return false;
	}
	size_t i = 0;
	if (s[0] == '-') {
		i = 1;
	}
	size_t digits = 0;
	while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
		i++;
		digits++;
	}
	if (i < s.size() && s[i] == '.') {
		i++;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
			i++;
			digits++;
		}
	}
	return digits > 0 && i == s.size();
}

// 解析 emit(<ident>) 信号发射（v1：零参，无参数列表）。成功 true，失败写 err。
// part 已保证以 "emit(" 开头（调用方检查）。
static bool parse_emit(const std::string &part, Effect &out, std::string &err) {
	size_t close = part.find(')', 5);
	if (close == std::string::npos) {
		err = "unterminated emit(...) in '" + part + "'";
		return false;
	}
	const std::string inner = trim(part.substr(5, close - 5));
	if (trim(part.substr(close + 1)) != "") {
		err = "unexpected text after emit(...) in '" + part + "'";
		return false;
	}
	if (inner.empty()) {
		err = "emit() requires a signal name";
		return false;
	}
	if (!is_ident(inner)) {
		err = "invalid signal name '" + inner + "' (emit v1: zero-arg signal only)";
		return false;
	}
	out.kind = EffectKind::Emit;
	out.signal_name = inner;
	return true;
}

// 解析字段引用：self.field / target.field / 裸 field（裸 = self）。v1 ontology 只支持 self 与 target。
static bool resolve_ref(const std::string &ref, RefOwner &owner, std::string &field, std::string &err) {
	size_t dot = ref.find('.');
	if (dot == std::string::npos) {
		owner = RefOwner::Self;
		field = ref;
	} else {
		const std::string owner_str = ref.substr(0, dot);
		if (owner_str == "self") {
			owner = RefOwner::Self;
		} else if (owner_str == "target") {
			owner = RefOwner::Target;
		} else {
			err = "unknown ref owner '" + owner_str + "' (ontology v1: self or target)";
			return false;
		}
		field = ref.substr(dot + 1);
	}
	if (!is_ident(field)) {
		err = "invalid field reference '" + ref + "'";
		return false;
	}
	return true;
}

// 找比较运算符（>= <= == != 优先，然后 > <）。返回位置与符号，找不到返回 false。
static bool find_cmp(const std::string &s, size_t &op_pos, std::string &cmp) {
	for (size_t i = 0; i + 1 <= s.size(); i++) {
		std::string two = s.substr(i, 2);
		if (two == ">=" || two == "<=" || two == "==" || two == "!=") {
			op_pos = i;
			cmp = two;
			return true;
		}
	}
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '>' || s[i] == '<') {
			op_pos = i;
			cmp = std::string(1, s[i]);
			return true;
		}
	}
	return false;
}

bool parse_guard(const std::string &when, Guard &out, std::string &err) {
	const std::string s = trim(when);
	size_t op_pos;
	std::string cmp;
	if (!find_cmp(s, op_pos, cmp)) {
		err = "missing comparison operator in guard '" + s + "'";
		return false;
	}
	const std::string lhs = trim(s.substr(0, op_pos));
	const std::string rhs = trim(s.substr(op_pos + cmp.size()));

	std::string field;
	RefOwner owner;
	if (!resolve_ref(lhs, owner, field, err)) {
		return false;
	}
	if (!is_number_literal(rhs)) {
		err = "non-numeric guard value '" + rhs + "'";
		return false;
	}
	out.ref = owner;
	out.field = field;
	out.cmp = cmp;
	out.value = rhs;
	return true;
}

bool parse_effects(const std::string &then, std::vector<Effect> &out, std::string &err) {
	out.clear();
	std::string s = trim(then);
	size_t start = 0;
	while (start <= s.size()) {
		size_t comma = s.find(',', start);
		std::string part = trim(s.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
		if (part.empty()) {
			err = "empty effect (check commas)";
			return false;
		}

		// emit(<ident>)：零参信号发射（ontology v1）。参数列表留 S6.2c+。
		if (part.compare(0, 5, "emit(") == 0) {
			Effect fx;
			if (!parse_emit(part, fx, err)) {
				return false;
			}
			out.push_back(fx);
			if (comma == std::string::npos) {
				break;
			}
			start = comma + 1;
			continue;
		}

		// 找赋值运算符：+= -= 优先，然后 =
		size_t op_pos = std::string::npos;
		EffectKind kind = EffectKind::Set;
		for (size_t i = 0; i + 1 < part.size(); i++) {
			if (part[i] == '+' && part[i + 1] == '=') {
				op_pos = i;
				kind = EffectKind::Add;
				break;
			}
			if (part[i] == '-' && part[i + 1] == '=') {
				op_pos = i;
				kind = EffectKind::Sub;
				break;
			}
		}
		size_t op_len = 1;
		if (op_pos == std::string::npos) {
			op_pos = part.find('=');
			if (op_pos == std::string::npos) {
				err = "expected assignment in effect '" + part + "'";
				return false;
			}
		} else {
			op_len = 2;
		}

		const std::string lhs = trim(part.substr(0, op_pos));
		const std::string rhs = trim(part.substr(op_pos + op_len));

		Effect fx;
		fx.kind = kind;
		if (!resolve_ref(lhs, fx.ref, fx.field, err)) {
			return false;
		}
		if (!is_number_literal(rhs)) {
			err = "non-numeric effect value '" + rhs + "'";
			return false;
		}
		fx.value = rhs;
		out.push_back(fx);

		if (comma == std::string::npos) {
			break;
		}
		start = comma + 1;
	}
	return true;
}

} // namespace gdsl
