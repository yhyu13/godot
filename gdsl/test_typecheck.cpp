#include "doctest.h"
#include "parser.h"
#include "typecheck.h"

static gdsl::Program parse(const char *src) {
	gdsl::Program p;
	std::string err;
	REQUIRE(gdsl::parse_program(src, p, err));
	return p;
}

TEST_CASE("[GDSL] Typecheck a valid program") {
	const gdsl::Program prog = parse(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"    speed: float = 400.0\n"
			"\n"
			"type Bullet @extends Area2D\n"
			"state:\n"
			"    damage: int = 1\n"
			"\n"
			"rule OnHit by Bullet:\n"
			"    when target.hp > 0\n"
			"    then target.hp -= 1, emit(hit)\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	REQUIRE(typed.types.size() == 2);
	CHECK(typed.types[0].name == "Player");
	REQUIRE(typed.types[0].fields.size() == 2);
	CHECK(typed.types[0].fields[0].type == gdsl::ValueType::Int);
	CHECK(typed.types[0].fields[1].type == gdsl::ValueType::Float);
	REQUIRE(typed.rules.size() == 1);
	CHECK(typed.rules[0].by == "Bullet");
}

TEST_CASE("[GDSL] Resolve a field referencing a declared type") {
	const gdsl::Program prog = parse(
			"type Weapon @extends Resource\n"
			"\n"
			"type Player @extends Node2D\n"
			"state:\n"
			"    weapon: Weapon = null\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	REQUIRE(typed.types.size() == 2);
	REQUIRE(typed.types[1].fields.size() == 1);
	CHECK(typed.types[1].fields[0].type == gdsl::ValueType::Named);
	CHECK(typed.types[1].fields[0].type_name == "Weapon");
}

TEST_CASE("[GDSL] Reject duplicate type name") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"\n"
			"type A @extends Node2D\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject unknown field type") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"state:\n"
			"    hp: bogus = 3\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject duplicate field name") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"    hp: int = 4\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject unknown trigger type in rule") {
	const gdsl::Program prog = parse(
			"type Player @extends Node2D\n"
			"\n"
			"rule OnHit by Ghost:\n"
			"    when target.hp > 0\n"
			"    then target.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}
