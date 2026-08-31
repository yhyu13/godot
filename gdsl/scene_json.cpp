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

static void collect_names(const NodeSpec &node, std::vector<std::string> &names) {
	names.push_back(node.name);
	for (const NodeSpec &child : node.children) {
		collect_names(child, names);
	}
}

static bool map_connection(const JsonValue &j, Connection &out, std::string &err) {
	if (j.type != JsonValue::Type::Object) {
		err = "connection must be an object";
		return false;
	}
	const JsonValue *signal = j.find("signal");
	const JsonValue *from = j.find("from");
	const JsonValue *to = j.find("to");
	const JsonValue *method = j.find("method");
	if (!signal || signal->type != JsonValue::Type::String) {
		err = "connection missing 'signal' string";
		return false;
	}
	if (!from || from->type != JsonValue::Type::String) {
		err = "connection missing 'from' string";
		return false;
	}
	if (!to || to->type != JsonValue::Type::String) {
		err = "connection missing 'to' string";
		return false;
	}
	if (!method || method->type != JsonValue::Type::String) {
		err = "connection missing 'method' string";
		return false;
	}
	out.signal = signal->string_value;
	out.from = from->string_value;
	out.to = to->string_value;
	out.method = method->string_value;
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
	if (const JsonValue *connections = root.find("connections")) {
		if (connections->type != JsonValue::Type::Array) {
			err = "'connections' must be an array";
			return false;
		}
		for (const JsonValue &c : connections->array) {
			Connection conn;
			if (!map_connection(c, conn, err)) {
				return false;
			}
			out.connections.push_back(std::move(conn));
		}
	}
	// 语义校验（FR-004）：connection 的 from/to 必须引用场景中已存在的节点。
	std::vector<std::string> names;
	collect_names(out.root, names);
	auto has_name = [&names](const std::string &n) {
		for (const std::string &existing : names) {
			if (existing == n) {
				return true;
			}
		}
		return false;
	};
	for (const Connection &c : out.connections) {
		if (!has_name(c.from)) {
			err = "connection references nonexistent node '" + c.from + "' (from)";
			return false;
		}
		if (!has_name(c.to)) {
			err = "connection references nonexistent node '" + c.to + "' (to)";
			return false;
		}
	}
	return true;
}

} // namespace gdsl
