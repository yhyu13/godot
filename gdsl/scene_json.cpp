#include "scene_json.h"
#include "json.h"
#include <utility>

namespace gdsl {

static std::string num_str(double d) {
	if (d == static_cast<double>(static_cast<long long>(d))) {
		return std::to_string(static_cast<long long>(d));
	}
	return std::to_string(d);
}

static bool serialize_prop(const JsonValue &v, std::string &out, std::string &err) {
	switch (v.type) {
		case JsonValue::Type::Number:
			out = num_str(v.number_value);
			return true;
		case JsonValue::Type::String:
			out = "\"" + v.string_value + "\"";
			return true;
		case JsonValue::Type::Bool:
			out = v.bool_value ? "true" : "false";
			return true;
		case JsonValue::Type::Object: {
			const JsonValue *x = v.find("x");
			const JsonValue *y = v.find("y");
			if (x && y && x->type == JsonValue::Type::Number && y->type == JsonValue::Type::Number) {
				out = "Vector2(" + num_str(x->number_value) + ", " + num_str(y->number_value) + ")";
				return true;
			}
			err = "unsupported prop object";
			return false;
		}
		default:
			err = "unsupported prop value";
			return false;
	}
}

static bool map_node(const JsonValue &j, NodeSpec &out, std::string &err) {
	if (j.type != JsonValue::Type::Object) {
		err = "node must be an object";
		return false;
	}
	const JsonValue *type = j.find("type");
	const JsonValue *name = j.find("name");
	if (!type || type->type != JsonValue::Type::String) {
		err = "node missing 'type' string";
		return false;
	}
	if (!name || name->type != JsonValue::Type::String) {
		err = "node missing 'name' string";
		return false;
	}
	out.type = type->string_value;
	out.name = name->string_value;
	out.props.clear();
	out.children.clear();

	if (const JsonValue *props = j.find("props")) {
		if (props->type != JsonValue::Type::Object) {
			err = "'props' must be an object";
			return false;
		}
		for (const auto &kv : props->object) {
			Prop p;
			p.name = kv.first;
			if (!serialize_prop(kv.second, p.value, err)) {
				return false;
			}
			out.props.push_back(std::move(p));
		}
	}
	if (const JsonValue *children = j.find("children")) {
		if (children->type != JsonValue::Type::Array) {
			err = "'children' must be an array";
			return false;
		}
		for (const JsonValue &c : children->array) {
			NodeSpec child;
			if (!map_node(c, child, err)) {
				return false;
			}
			out.children.push_back(std::move(child));
		}
	}
	return true;
}

bool scene_from_json(const std::string &json, SceneSpec &out, std::string &err) {
	JsonValue root;
	if (!parse_json(json, root, err)) {
		return false;
	}
	if (root.type != JsonValue::Type::Object) {
		err = "scene must be an object";
		return false;
	}
	const JsonValue *r = root.find("root");
	if (!r) {
		err = "missing 'root'";
		return false;
	}
	if (!map_node(*r, out.root, err)) {
		return false;
	}
	if (const JsonValue *children = root.find("children")) {
		if (children->type != JsonValue::Type::Array) {
			err = "'children' must be an array";
			return false;
		}
		for (const JsonValue &c : children->array) {
			NodeSpec child;
			if (!map_node(c, child, err)) {
				return false;
			}
			out.root.children.push_back(std::move(child));
		}
	}
	return true;
}

} // namespace gdsl
