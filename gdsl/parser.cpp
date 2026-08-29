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

} // namespace gdsl
