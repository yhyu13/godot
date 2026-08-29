#include "parser.h"

namespace gdsl {

static std::string trim(const std::string &s) {
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) {
		return "";
	}
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

static std::vector<std::string> split_lines(const std::string &text) {
	std::vector<std::string> lines;
	size_t start = 0;
	while (start <= text.size()) {
		size_t nl = text.find('\n', start);
		if (nl == std::string::npos) {
			lines.push_back(text.substr(start));
			break;
		}
		lines.push_back(text.substr(start, nl - start));
		start = nl + 1;
	}
	return lines;
}

bool parse_type_decl(const std::string &line, TypeDecl &out, std::string &err) {
	const std::string s = trim(line);
	const std::string kw = "type ";
	if (s.compare(0, kw.size(), kw) != 0) {
		err = "expected 'type'";
		return false;
	}
	size_t pos = kw.size();

	// 类型名：到空白或 '@' 为止
	size_t name_end = s.find_first_of(" \t@", pos);
	if (name_end == std::string::npos || name_end == pos) {
		err = "missing type name";
		return false;
	}
	out.name = s.substr(pos, name_end - pos);
	pos = s.find_first_not_of(" \t", name_end);
	if (pos == std::string::npos) {
		err = "missing @extends";
		return false;
	}

	const std::string ext = "@extends";
	if (s.compare(pos, ext.size(), ext) != 0) {
		err = "expected '@extends'";
		return false;
	}
	pos += ext.size();

	size_t base_start = s.find_first_not_of(" \t", pos);
	if (base_start == std::string::npos) {
		err = "missing base type";
		return false;
	}
	size_t base_end = s.find_first_of(" \t", base_start);
	out.base = s.substr(base_start, base_end == std::string::npos ? std::string::npos : base_end - base_start);
	return true;
}

bool parse_state_field(const std::string &line, FieldDecl &out, std::string &err) {
	const std::string s = trim(line);
	size_t colon = s.find(':');
	if (colon == std::string::npos) {
		err = "missing ':'";
		return false;
	}
	const std::string name = trim(s.substr(0, colon));
	if (name.empty()) {
		err = "missing field name";
		return false;
	}
	out.name = name;

	size_t eq = s.find('=', colon + 1);
	if (eq == std::string::npos) {
		err = "missing '='";
		return false;
	}
	const std::string type = trim(s.substr(colon + 1, eq - colon - 1));
	if (type.empty()) {
		err = "missing type";
		return false;
	}
	out.type = type;

	const std::string dv = trim(s.substr(eq + 1));
	if (dv.empty()) {
		err = "missing default value";
		return false;
	}
	out.default_value = dv;
	return true;
}

bool parse_type_block(const std::string &text, TypeDecl &out, std::string &err) {
	const std::vector<std::string> lines = split_lines(text);
	size_t i = 0;
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		err = "empty type block";
		return false;
	}
	if (!parse_type_decl(lines[i], out, err)) {
		return false;
	}
	i++;
	out.fields.clear();

	// 跳过空行，看是否有 "state:" 块
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		return true; // 无 state 块
	}
	if (trim(lines[i]) != "state:") {
		err = "expected 'state:'";
		return false;
	}
	i++;
	while (i < lines.size()) {
		const std::string l = trim(lines[i]);
		if (l.empty()) {
			i++;
			continue;
		}
		FieldDecl f;
		if (!parse_state_field(lines[i], f, err)) {
			return false;
		}
		out.fields.push_back(f);
		i++;
	}
	return true;
}

bool parse_rule_block(const std::string &text, RuleDecl &out, std::string &err) {
	const std::vector<std::string> lines = split_lines(text);
	size_t i = 0;
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		err = "empty rule block";
		return false;
	}

	// 1. rule 头："rule Name by Source:"
	const std::string header = trim(lines[i]);
	const std::string kw = "rule ";
	if (header.compare(0, kw.size(), kw) != 0) {
		err = "expected 'rule'";
		return false;
	}
	size_t pos = kw.size();
	size_t name_end = header.find_first_of(" 	", pos);
	if (name_end == std::string::npos || name_end == pos) {
		err = "missing rule name";
		return false;
	}
	out.name = header.substr(pos, name_end - pos);
	pos = header.find_first_not_of(" 	", name_end);
	if (pos == std::string::npos) {
		err = "missing 'by'";
		return false;
	}
	if (header.compare(pos, 2, "by") != 0) {
		err = "expected 'by'";
		return false;
	}
	pos += 2;
	pos = header.find_first_not_of(" 	", pos);
	if (pos == std::string::npos) {
		err = "missing source type";
		return false;
	}
	size_t by_end = header.find_first_of(" 	:", pos);
	out.by = header.substr(pos, by_end == std::string::npos ? std::string::npos : by_end - pos);
	i++;

	// 2. when 行
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		err = "missing 'when'";
		return false;
	}
	const std::string when_line = trim(lines[i]);
	const std::string when_kw = "when ";
	if (when_line.compare(0, when_kw.size(), when_kw) != 0) {
		err = "expected 'when'";
		return false;
	}
	out.when = trim(when_line.substr(when_kw.size()));
	if (out.when.empty()) {
		err = "missing when expression";
		return false;
	}
	i++;

	// 3. then 行
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		err = "missing 'then'";
		return false;
	}
	const std::string then_line = trim(lines[i]);
	const std::string then_kw = "then ";
	if (then_line.compare(0, then_kw.size(), then_kw) != 0) {
		err = "expected 'then'";
		return false;
	}
	out.then = trim(then_line.substr(then_kw.size()));
	if (out.then.empty()) {
		err = "missing then effects";
		return false;
	}
	return true;
}

bool parse_program(const std::string &source, Program &out, std::string &err) {
	const std::vector<std::string> lines = split_lines(source);
	out.types.clear();
	out.rules.clear();
	size_t i = 0;
	while (i < lines.size()) {
		const std::string l = trim(lines[i]);
		if (l.empty()) {
			i++;
			continue;
		}
		const bool is_type = l.compare(0, 5, "type ") == 0;
		const bool is_rule = l.compare(0, 5, "rule ") == 0;
		if (!is_type && !is_rule) {
			err = "unexpected line";
			return false;
		}
		// 收集块：到空行或下一个块关键字为止
		size_t end = i;
		while (end < lines.size() && !trim(lines[end]).empty()) {
			const std::string cur = trim(lines[end]);
			if (end > i && (cur.compare(0, 5, "type ") == 0 || cur.compare(0, 5, "rule ") == 0)) {
				break;
			}
			end++;
		}
		std::string block;
		for (size_t k = i; k < end; k++) {
			block += lines[k];
			block += '\n';
		}
		if (is_type) {
			TypeDecl t;
			if (!parse_type_block(block, t, err)) {
				return false;
			}
			out.types.push_back(t);
		} else {
			RuleDecl r;
			if (!parse_rule_block(block, r, err)) {
				return false;
			}
			out.rules.push_back(r);
		}
		i = end;
	}
	return true;
}

} // namespace gdsl
