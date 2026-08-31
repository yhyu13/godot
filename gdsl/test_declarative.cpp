#include "doctest.h"
#include "codegen_declarative.h"
#include "scene_json.h"

TEST_CASE("[GDSL] Emit a minimal tscn with root and child") {
	gdsl::SceneSpec spec;
	spec.root.type = "Node2D";
	spec.root.name = "Main";
	gdsl::NodeSpec icon;
	icon.type = "Sprite2D";
	icon.name = "icon";
	icon.props.push_back({ "position", "Vector2(10, 20)" });
	spec.root.children.push_back(icon);

	const std::string out = gdsl::emit_tscn(spec);
	CHECK(out.find("[gd_scene format=3]") != std::string::npos);
	CHECK(out.find("[node name=\"Main\" type=\"Node2D\"]") != std::string::npos);
	CHECK(out.find("[node name=\"icon\" type=\"Sprite2D\" parent=\".\"]") != std::string::npos);
	CHECK(out.find("position = Vector2(10, 20)") != std::string::npos);
}

TEST_CASE("[GDSL] Emit tscn exact golden output") {
	gdsl::SceneSpec spec;
	spec.root.type = "Node2D";
	spec.root.name = "Main";
	gdsl::NodeSpec icon;
	icon.type = "Sprite2D";
	icon.name = "icon";
	icon.props.push_back({ "position", "Vector2(10, 20)" });
	spec.root.children.push_back(icon);

	const std::string expected =
			"[gd_scene format=3]\n"
			"\n"
			"[node name=\"Main\" type=\"Node2D\"]\n"
			"\n"
			"[node name=\"icon\" type=\"Sprite2D\" parent=\".\"]\n"
			"position = Vector2(10, 20)\n";
	CHECK(gdsl::emit_tscn(spec) == expected);
}

TEST_CASE("[GDSL] Emit tscn is deterministic") {
	gdsl::SceneSpec spec;
	spec.root.type = "Node2D";
	spec.root.name = "Root";
	gdsl::NodeSpec child;
	child.type = "Node2D";
	child.name = "Child";
	spec.root.children.push_back(child);

	const std::string a = gdsl::emit_tscn(spec);
	const std::string b = gdsl::emit_tscn(spec);
	CHECK_FALSE(a.empty());
	CHECK(a == b);
}

TEST_CASE("[GDSL] Parse a scene JSON recipe") {
	gdsl::SceneSpec spec;
	std::string err;
	const std::string json =
			"{\"root\":{\"type\":\"Node2D\",\"name\":\"Main\"},"
			"\"children\":[{\"type\":\"Sprite2D\",\"name\":\"icon\","
			"\"props\":{\"position\":{\"x\":10,\"y\":20}}}]}";
	bool ok = gdsl::scene_from_json(json, spec, err);
	REQUIRE(ok);
	CHECK(spec.root.type == "Node2D");
	CHECK(spec.root.name == "Main");
	REQUIRE(spec.root.children.size() == 1);
	CHECK(spec.root.children[0].type == "Sprite2D");
	CHECK(spec.root.children[0].name == "icon");
	REQUIRE(spec.root.children[0].props.size() == 1);
	CHECK(spec.root.children[0].props[0].name == "position");
	CHECK(spec.root.children[0].props[0].value == "Vector2(10, 20)");
}

TEST_CASE("[GDSL] Reject malformed scene JSON") {
	gdsl::SceneSpec spec;
	std::string err;
	bool ok = gdsl::scene_from_json("{\"root\": {\"type\": \"Node2D\", \"name\": \"Main\"", spec, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject scene JSON missing root") {
	gdsl::SceneSpec spec;
	std::string err;
	bool ok = gdsl::scene_from_json("{\"children\": []}", spec, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject node missing type") {
	gdsl::SceneSpec spec;
	std::string err;
	bool ok = gdsl::scene_from_json("{\"root\":{\"name\":\"Main\"}}", spec, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] End-to-end: scene JSON → tscn") {
	gdsl::SceneSpec spec;
	std::string err;
	const std::string json =
			"{\"root\":{\"type\":\"Node2D\",\"name\":\"Main\"},"
			"\"children\":[{\"type\":\"Sprite2D\",\"name\":\"icon\","
			"\"props\":{\"position\":{\"x\":10,\"y\":20}}}]}";
	REQUIRE(gdsl::scene_from_json(json, spec, err));
	const std::string tscn = gdsl::emit_tscn(spec);
	CHECK(tscn.find("position = Vector2(10, 20)") != std::string::npos);
	CHECK(tscn.find("[node name=\"icon\" type=\"Sprite2D\" parent=\".\"]") != std::string::npos);
}

TEST_CASE("[GDSL] Emit connection block in tscn") {
	gdsl::SceneSpec spec;
	spec.root.type = "Node2D";
	spec.root.name = "Main";
	gdsl::NodeSpec player;
	player.type = "CharacterBody2D";
	player.name = "Player";
	spec.root.children.push_back(player);
	gdsl::Connection c;
	c.signal = "hit";
	c.from = "Player";
	c.to = "Main";
	c.method = "_on_player_hit";
	spec.connections.push_back(c);

	const std::string out = gdsl::emit_tscn(spec);
	CHECK(out.find("[connection signal=\"hit\" from=\"Player\" to=\"Main\" method=\"_on_player_hit\"]") != std::string::npos);
}

TEST_CASE("[GDSL] Parse a connection from scene JSON") {
	gdsl::SceneSpec spec;
	std::string err;
	const std::string json =
			"{\"root\":{\"type\":\"Node2D\",\"name\":\"Main\"},"
			"\"connections\":[{\"signal\":\"hit\",\"from\":\"Main\",\"to\":\"Main\",\"method\":\"_on_hit\"}]}";
	REQUIRE(gdsl::scene_from_json(json, spec, err));
	REQUIRE(spec.connections.size() == 1);
	CHECK(spec.connections[0].signal == "hit");
	CHECK(spec.connections[0].from == "Main");
	CHECK(spec.connections[0].to == "Main");
	CHECK(spec.connections[0].method == "_on_hit");
}

TEST_CASE("[GDSL] Reject connection referencing a nonexistent node") {
	gdsl::SceneSpec spec;
	std::string err;
	const std::string json =
			"{\"root\":{\"type\":\"Node2D\",\"name\":\"Main\"},"
			"\"connections\":[{\"signal\":\"hit\",\"from\":\"Ghost\",\"to\":\"Main\",\"method\":\"_on_hit\"}]}";
	CHECK_FALSE(gdsl::scene_from_json(json, spec, err));
	CHECK_FALSE(err.empty());
}
