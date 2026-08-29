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
