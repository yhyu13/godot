// gdsl/json.h — 最小 JSON 解析器（独立内核，无第三方依赖）
// 仅覆盖 JSON 标准子集：object/array/string/number/bool/null。
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace gdsl {

struct JsonValue {
	enum class Type { Null, Bool, Number, String, Array, Object };
	Type type = Type::Null;
	bool bool_value = false;
	double number_value = 0.0;
	std::string string_value;
	std::vector<JsonValue> array;
	std::vector<std::pair<std::string, JsonValue>> object;

	// 对象取值；key 不存在或非对象返回 nullptr。
	const JsonValue *find(const std::string &key) const;
};

// 解析 JSON 文本；失败返回 false 并写 err。
bool parse_json(const std::string &text, JsonValue &out, std::string &err);

} // namespace gdsl
