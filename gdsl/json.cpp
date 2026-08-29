#include "json.h"
#include <cstdlib>
#include <cstring>

namespace gdsl {

const JsonValue *JsonValue::find(const std::string &key) const {
	if (type != Type::Object) {
		return nullptr;
	}
	for (const auto &kv : object) {
		if (kv.first == key) {
			return &kv.second;
		}
	}
	return nullptr;
}

namespace {

struct Parser {
	const std::string &s;
	size_t i = 0;
	std::string err;

	explicit Parser(const std::string &t) : s(t) {}

	void skip_ws() {
		while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
			i++;
		}
	}

	static bool is_digit(char c) {
		return c >= '0' && c <= '9';
	}

	bool fail(const std::string &m) {
		err = m;
		return false;
	}

	bool parse_value(JsonValue &v) {
		skip_ws();
		if (i >= s.size()) {
			return fail("unexpected end of input");
		}
		switch (s[i]) {
			case '{': return parse_object(v);
			case '[': return parse_array(v);
			case '"':
				v.type = JsonValue::Type::String;
				return parse_string(v.string_value);
			case 't': return parse_lit("true", v, JsonValue::Type::Bool, true);
			case 'f': return parse_lit("false", v, JsonValue::Type::Bool, false);
			case 'n': return parse_lit("null", v, JsonValue::Type::Null, false);
			default:
				if (s[i] == '-' || is_digit(s[i])) {
					return parse_number(v);
				}
				return fail("unexpected character");
		}
	}

	bool parse_lit(const char *lit, JsonValue &v, JsonValue::Type t, bool bv) {
		const size_t n = strlen(lit);
		if (s.compare(i, n, lit) != 0) {
			return fail("invalid literal");
		}
		i += n;
		v.type = t;
		if (t == JsonValue::Type::Bool) {
			v.bool_value = bv;
		}
		return true;
	}

	bool parse_object(JsonValue &v) {
		v.type = JsonValue::Type::Object;
		i++; // consume '{'
		skip_ws();
		if (i < s.size() && s[i] == '}') {
			i++;
			return true;
		}
		while (true) {
			skip_ws();
			if (i >= s.size() || s[i] != '"') {
				return fail("expected string key");
			}
			std::string key;
			if (!parse_string(key)) {
				return false;
			}
			skip_ws();
			if (i >= s.size() || s[i] != ':') {
				return fail("expected ':'");
			}
			i++;
			JsonValue val;
			if (!parse_value(val)) {
				return false;
			}
			v.object.emplace_back(std::move(key), std::move(val));
			skip_ws();
			if (i >= s.size()) {
				return fail("unterminated object");
			}
			if (s[i] == ',') {
				i++;
				continue;
			}
			if (s[i] == '}') {
				i++;
				return true;
			}
			return fail("expected ',' or '}'");
		}
	}

	bool parse_array(JsonValue &v) {
		v.type = JsonValue::Type::Array;
		i++; // consume '['
		skip_ws();
		if (i < s.size() && s[i] == ']') {
			i++;
			return true;
		}
		while (true) {
			JsonValue val;
			if (!parse_value(val)) {
				return false;
			}
			v.array.push_back(std::move(val));
			skip_ws();
			if (i >= s.size()) {
				return fail("unterminated array");
			}
			if (s[i] == ',') {
				i++;
				continue;
			}
			if (s[i] == ']') {
				i++;
				return true;
			}
			return fail("expected ',' or ']'");
		}
	}

	bool parse_string(std::string &out) {
		if (i >= s.size() || s[i] != '"') {
			return fail("expected '\"'");
		}
		i++;
		out.clear();
		while (true) {
			if (i >= s.size()) {
				return fail("unterminated string");
			}
			const char c = s[i];
			if (c == '"') {
				i++;
				return true;
			}
			if (c == '\\') {
				i++;
				if (i >= s.size()) {
					return fail("unterminated escape");
				}
				const char e = s[i];
				switch (e) {
					case '"': out += '"'; i++; break;
					case '\\': out += '\\'; i++; break;
					case '/': out += '/'; i++; break;
					case 'b': out += '\b'; i++; break;
					case 'f': out += '\f'; i++; break;
					case 'n': out += '\n'; i++; break;
					case 'r': out += '\r'; i++; break;
					case 't': out += '\t'; i++; break;
					case 'u': {
						if (i + 4 >= s.size()) {
							return fail("bad \\u escape");
						}
						unsigned cp = 0;
						for (int k = 0; k < 4; k++) {
							const char h = s[i + 1 + k];
							cp <<= 4;
							if (h >= '0' && h <= '9') {
								cp |= static_cast<unsigned>(h - '0');
							} else if (h >= 'a' && h <= 'f') {
								cp |= static_cast<unsigned>(h - 'a' + 10);
							} else if (h >= 'A' && h <= 'F') {
								cp |= static_cast<unsigned>(h - 'A' + 10);
							} else {
								return fail("bad hex in \\u escape");
							}
						}
						i += 5;
						append_utf8(out, cp);
						break;
					}
					default:
						return fail("invalid escape");
				}
			} else {
				out += c;
				i++;
			}
		}
	}

	static void append_utf8(std::string &out, unsigned cp) {
		if (cp < 0x80) {
			out += static_cast<char>(cp);
		} else if (cp < 0x800) {
			out += static_cast<char>(0xC0 | (cp >> 6));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		} else {
			out += static_cast<char>(0xE0 | (cp >> 12));
			out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
	}

	bool parse_number(JsonValue &v) {
		const size_t start = i;
		if (i < s.size() && s[i] == '-') {
			i++;
		}
		if (i >= s.size() || !is_digit(s[i])) {
			return fail("invalid number");
		}
		while (i < s.size() && is_digit(s[i])) {
			i++;
		}
		if (i < s.size() && s[i] == '.') {
			i++;
			if (i >= s.size() || !is_digit(s[i])) {
				return fail("invalid number fraction");
			}
			while (i < s.size() && is_digit(s[i])) {
				i++;
			}
		}
		if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
			i++;
			if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
				i++;
			}
			if (i >= s.size() || !is_digit(s[i])) {
				return fail("invalid number exponent");
			}
			while (i < s.size() && is_digit(s[i])) {
				i++;
			}
		}
		v.type = JsonValue::Type::Number;
		v.number_value = strtod(s.c_str() + start, nullptr);
		return true;
	}
};

} // namespace

bool parse_json(const std::string &text, JsonValue &out, std::string &err) {
	Parser p(text);
	if (!p.parse_value(out)) {
		err = p.err;
		return false;
	}
	p.skip_ws();
	if (p.i != text.size()) {
		err = "trailing characters after JSON";
		return false;
	}
	return true;
}

} // namespace gdsl
