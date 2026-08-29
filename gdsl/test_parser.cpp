#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "parser.h"

TEST_CASE("[GDSL] Parse a type declaration") {
	gdsl::TypeDecl decl;
	std::string err;
	bool ok = gdsl::parse_type_decl("type Player @extends CharacterBody2D", decl, err);
	REQUIRE(ok);
	CHECK(decl.name == "Player");
	CHECK(decl.base == "CharacterBody2D");
}

TEST_CASE("[GDSL] Reject a type declaration without @extends") {
	gdsl::TypeDecl decl;
	std::string err;
	bool ok = gdsl::parse_type_decl("type Player", decl, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject a type declaration missing base type") {
	gdsl::TypeDecl decl;
	std::string err;
	bool ok = gdsl::parse_type_decl("type Player @extends", decl, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject a line that is not a type declaration") {
	gdsl::TypeDecl decl;
	std::string err;
	bool ok = gdsl::parse_type_decl("rule OnHit by Bullet:", decl, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Parse a state field") {
	gdsl::FieldDecl f;
	std::string err;
	bool ok = gdsl::parse_state_field("hp: int = 3", f, err);
	REQUIRE(ok);
	CHECK(f.name == "hp");
	CHECK(f.type == "int");
	CHECK(f.default_value == "3");
}

TEST_CASE("[GDSL] Parse a float state field") {
	gdsl::FieldDecl f;
	std::string err;
	bool ok = gdsl::parse_state_field("speed: float = 400.0", f, err);
	REQUIRE(ok);
	CHECK(f.name == "speed");
	CHECK(f.type == "float");
	CHECK(f.default_value == "400.0");
}

TEST_CASE("[GDSL] Reject a state field missing colon") {
	gdsl::FieldDecl f;
	std::string err;
	bool ok = gdsl::parse_state_field("hp int = 3", f, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject a state field missing type") {
	gdsl::FieldDecl f;
	std::string err;
	bool ok = gdsl::parse_state_field("hp: = 3", f, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject a state field missing default value") {
	gdsl::FieldDecl f;
	std::string err;
	bool ok = gdsl::parse_state_field("hp: int =", f, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Parse a type block with state fields") {
	gdsl::TypeDecl decl;
	std::string err;
	bool ok = gdsl::parse_type_block(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"    speed: float = 400.0\n",
			decl, err);
	REQUIRE(ok);
	CHECK(decl.name == "Player");
	CHECK(decl.base == "CharacterBody2D");
	REQUIRE(decl.fields.size() == 2);
	CHECK(decl.fields[0].name == "hp");
	CHECK(decl.fields[0].type == "int");
	CHECK(decl.fields[0].default_value == "3");
	CHECK(decl.fields[1].name == "speed");
	CHECK(decl.fields[1].type == "float");
	CHECK(decl.fields[1].default_value == "400.0");
}

TEST_CASE("[GDSL] Parse a type block with no state block") {
	gdsl::TypeDecl decl;
	std::string err;
	bool ok = gdsl::parse_type_block("type Enemy @extends Node2D", decl, err);
	REQUIRE(ok);
	CHECK(decl.name == "Enemy");
	CHECK(decl.base == "Node2D");
	CHECK(decl.fields.empty());
}

TEST_CASE("[GDSL] Reject a type block with a malformed state field") {
	gdsl::TypeDecl decl;
	std::string err;
	bool ok = gdsl::parse_type_block(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp int = 3\n",
			decl, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Parse a rule block") {
	gdsl::RuleDecl rule;
	std::string err;
	bool ok = gdsl::parse_rule_block(
			"rule OnHit by Bullet:\n"
			"    when target.hp > 0\n"
			"    then target.hp -= 1, emit(hit)\n",
			rule, err);
	REQUIRE(ok);
	CHECK(rule.name == "OnHit");
	CHECK(rule.by == "Bullet");
	CHECK(rule.when == "target.hp > 0");
	CHECK(rule.then == "target.hp -= 1, emit(hit)");
}

TEST_CASE("[GDSL] Reject a rule block missing when") {
	gdsl::RuleDecl rule;
	std::string err;
	bool ok = gdsl::parse_rule_block(
			"rule OnHit by Bullet:\n"
			"    then target.hp -= 1\n",
			rule, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject a rule block missing then") {
	gdsl::RuleDecl rule;
	std::string err;
	bool ok = gdsl::parse_rule_block(
			"rule OnHit by Bullet:\n"
			"    when target.hp > 0\n",
			rule, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Parse a full program with a type and a rule") {
	gdsl::Program prog;
	std::string err;
	bool ok = gdsl::parse_program(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule OnHit by Bullet:\n"
			"    when target.hp > 0\n"
			"    then target.hp -= 1, emit(hit)\n",
			prog, err);
	REQUIRE(ok);
	REQUIRE(prog.types.size() == 1);
	CHECK(prog.types[0].name == "Player");
	REQUIRE(prog.types[0].fields.size() == 1);
	CHECK(prog.types[0].fields[0].name == "hp");
	REQUIRE(prog.rules.size() == 1);
	CHECK(prog.rules[0].name == "OnHit");
	CHECK(prog.rules[0].by == "Bullet");
	CHECK(prog.rules[0].when == "target.hp > 0");
}

TEST_CASE("[GDSL] Reject a program with an unexpected line") {
	gdsl::Program prog;
	std::string err;
	bool ok = gdsl::parse_program(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"bogus line here\n",
			prog, err);
	CHECK_FALSE(ok);
	CHECK_FALSE(err.empty());
}
