#include "parser.h"

namespace gdsl {

static std::string trim(const std::string &s) {
	size_t b = s.find_first_not_of(" 	\r\n");
	if (b == std::string::npos) {
		return "";
	}
	size_t e = s.find_last_not_of(" 	\r\n");
	return s.substr(b, e - b + 1);
}

// FR-009：报错定位——把「第几行」前置到错误信息里。
// line 是 0-based 的块内行号，加 1 成 1-based；line_offset 是块在程序里的起始行（0-based）。
static std::string at_line(size_t line, size_t line_offset, const std::string &msg) {
	return "line " + std::to_string(line + line_offset + 1) + ": " + msg;
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

// 解析可选 "signals: a, b" 信号声明行（FR-005：信号存在性校验的封闭词汇表）。
// 约定 signals: 独占一行（可用逗号分隔多个信号名）且不含 '='（否则会被当成 state 字段）。
static bool parse_signals(const std::string &line, TypeDecl &out) {
	const std::string s = trim(line);
	const std::string kw = "signals:";
	if (s.compare(0, kw.size(), kw) != 0 || s.find('=') != std::string::npos) {
		return false; // 不是 signals 注释行（可能是名为 signals 的字段）
	}
	const std::string rest = s.substr(kw.size());
	size_t pos = 0;
	while (pos <= rest.size()) {
		size_t comma = rest.find(',', pos);
		std::string tok = trim((comma == std::string::npos) ? rest.substr(pos) : rest.substr(pos, comma - pos));
		if (!tok.empty()) {
			out.signals.push_back(tok);
		}
		if (comma == std::string::npos) {
			break;
		}
		pos = comma + 1;
	}
	return true;
}

// 解析 type 块；line_offset = 块首行在源程序里的 0-based 起始行号，供 FR-009 报错定位。
bool parse_type_block(const std::string &text, TypeDecl &out, std::string &err, size_t line_offset) {
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
		err = at_line(i, line_offset, err);
		return false;
	}
	i++;
	out.fields.clear();
	out.signals.clear();

	// 可选 signals: 注释块（需在 "state:" 之前）
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i < lines.size() && parse_signals(lines[i], out)) {
		i++;
	}

	// 跳过空行，看是否有 "state:" 块
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		return true; // 无 state 块
	}
	if (trim(lines[i]) != "state:") {
		err = at_line(i, line_offset, "expected 'state:'");
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
			err = at_line(i, line_offset, err);
			return false;
		}
		out.fields.push_back(f);
		i++;
	}
	return true;
}

// 解析 rule 块；line_offset = 块首行在源程序里的 0-based 起始行号，供 FR-009 报错定位。
bool parse_rule_block(const std::string &text, RuleDecl &out, std::string &err, size_t line_offset) {
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
		err = at_line(i, line_offset, "expected 'rule'");
		return false;
	}
	size_t pos = kw.size();
	size_t name_end = header.find_first_of(" 	", pos);
	if (name_end == std::string::npos || name_end == pos) {
		err = at_line(i, line_offset, "missing rule name");
		return false;
	}
	out.name = header.substr(pos, name_end - pos);
	pos = header.find_first_not_of(" 	", name_end);
	if (pos == std::string::npos) {
		err = at_line(i, line_offset, "missing 'by'");
		return false;
	}
	if (header.compare(pos, 2, "by") != 0) {
		err = at_line(i, line_offset, "expected 'by'");
		return false;
	}
	pos += 2;
	pos = header.find_first_not_of(" 	", pos);
	if (pos == std::string::npos) {
		err = at_line(i, line_offset, "missing source type");
		return false;
	}
	size_t by_end = header.find_first_of(" 	:", pos);
	out.by = header.substr(pos, by_end == std::string::npos ? std::string::npos : by_end - pos);
	out.target.clear();

	// 可选 target 子句："target <Type>"（by 之后、':' 之前）。声明跨参与者类型。
	size_t rest = header.find_first_not_of(" 	", by_end);
	if (rest != std::string::npos && header[rest] != ':') {
		const std::string tk = "target ";
		if (header.compare(rest, tk.size(), tk) != 0) {
			err = at_line(i, line_offset, "expected ':' or 'target <Type>' after 'by <Type>'");
			return false;
		}
		size_t tpos = header.find_first_not_of(" 	", rest + tk.size());
		if (tpos == std::string::npos || header[tpos] == ':') {
			err = at_line(i, line_offset, "missing target type");
			return false;
		}
		size_t t_end = header.find_first_of(" 	:", tpos);
		out.target = header.substr(tpos, t_end == std::string::npos ? std::string::npos : t_end - tpos);
	}
	i++;

	// 2. when 行
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		err = at_line(i, line_offset, "missing 'when'");
		return false;
	}
	const std::string when_line = trim(lines[i]);
	const std::string when_kw = "when ";
	if (when_line.compare(0, when_kw.size(), when_kw) != 0) {
		err = at_line(i, line_offset, "expected 'when'");
		return false;
	}
	out.when = trim(when_line.substr(when_kw.size()));
	if (out.when.empty()) {
		err = at_line(i, line_offset, "missing when expression");
		return false;
	}
	i++;

	// 3. then 行
	while (i < lines.size() && trim(lines[i]).empty()) {
		i++;
	}
	if (i >= lines.size()) {
		err = at_line(i, line_offset, "missing 'then'");
		return false;
	}
	const std::string then_line = trim(lines[i]);
	const std::string then_kw = "then ";
	if (then_line.compare(0, then_kw.size(), then_kw) != 0) {
		err = at_line(i, line_offset, "expected 'then'");
		return false;
	}
	out.then = trim(then_line.substr(then_kw.size()));
	if (out.then.empty()) {
		err = at_line(i, line_offset, "missing then effects");
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
			err = at_line(i, 0, "unexpected line '" + l + "'");
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
			if (!parse_type_block(block, t, err, i)) {
				return false;
			}
			out.types.push_back(t);
		} else {
			RuleDecl r;
			if (!parse_rule_block(block, r, err, i)) {
				return false;
			}
			out.rules.push_back(r);
		}
		i = end;
	}
	return true;
}

} // namespace gdsl
