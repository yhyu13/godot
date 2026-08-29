#include "codegen_declarative.h"

namespace gdsl {

static void emit_node(const NodeSpec &node, const std::string &parent, std::string &out) {
	out += "\n[node name=\"" + node.name + "\" type=\"" + node.type + "\"";
	if (!parent.empty()) {
		out += " parent=\"" + parent + "\"";
	}
	out += "]\n";
	for (const Prop &p : node.props) {
		out += p.name + " = " + p.value + "\n";
	}
	for (const NodeSpec &child : node.children) {
		emit_node(child, ".", out);
	}
}

std::string emit_tscn(const SceneSpec &spec) {
	std::string out = "[gd_scene format=3]\n";
	emit_node(spec.root, "", out);
	return out;
}

} // namespace gdsl
