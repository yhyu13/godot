// gdsl/codegen_declarative.h — 声明式层：SceneSpec → .tscn 文本
// 设计锚点：doc_ai/GODOT_LLM_DSL_DESIGN.md §3.1、PLAN §2（S1 tracer bullet）
#pragma once
#include <string>
#include <vector>

namespace gdsl {

struct Prop {
	std::string name;
	std::string value; // 已序列化的 Godot 字面量，如 "Vector2(10, 20)"
};

struct NodeSpec {
	std::string type; // 节点类型，如 "Node2D"、"Sprite2D"
	std::string name; // 节点名，如 "Main"、"icon"
	std::vector<Prop> props;
	std::vector<NodeSpec> children;
};

struct Connection {
	std::string signal; // 信号名，如 "hit"
	std::string from; // 发射方节点名
	std::string to; // 接收方节点名
	std::string method; // 接收方方法名，如 "_on_player_hit"
};

struct SceneSpec {
	NodeSpec root; // 根节点，整棵树挂在 root.children 下
	std::vector<Connection> connections; // 信号连接（[connection] 块）
};

// 把场景配方编译成 .tscn 文本（确定性：同一输入逐字节同一输出）。
std::string emit_tscn(const SceneSpec &spec);

} // namespace gdsl
