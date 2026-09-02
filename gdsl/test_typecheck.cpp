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
			"    when self.damage > 0\n"
			"    then self.damage -= 1\n");
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
			"    when self.hp > 0\n"
			"    then self.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Typecheck fills structured guard and effects") {
	const gdsl::Program prog = parse(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule TakeDamage by Player:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	REQUIRE(typed.rules.size() == 1);
	CHECK(typed.rules[0].guard.field == "hp");
	CHECK(typed.rules[0].guard.cmp == ">");
	CHECK(typed.rules[0].guard.value == "0");
	REQUIRE(typed.rules[0].effects.size() == 1);
	CHECK(typed.rules[0].effects[0].kind == gdsl::EffectKind::Sub);
	CHECK(typed.rules[0].effects[0].field == "hp");
	CHECK(typed.rules[0].effects[0].value == "1");
}

TEST_CASE("[GDSL] Reject rule referencing unknown field") {
	const gdsl::Program prog = parse(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule Heal by Player:\n"
			"    when self.mana > 0\n"
			"    then self.hp += 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Typecheck rule with emit_signal effect") {
	const gdsl::Program prog = parse(
			"type Player @extends Node2D\n"
			"    signals: died\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule Die by Player:\n"
			"    when self.hp <= 0\n"
			"    then emit(died)\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	REQUIRE(typed.rules.size() == 1);
	REQUIRE(typed.rules[0].effects.size() == 1);
	CHECK(typed.rules[0].effects[0].kind == gdsl::EffectKind::Emit);
	CHECK(typed.rules[0].effects[0].signal_name == "died");
}

// FR-005 mutation guard (typo_signal): 拼错的信号名必须被 typecheck 拒收，否则 codegen 会注册一个
// 无名信号、连接方 connect 该名静默失败（silent no-op）。这是可验证性真实缺口。
TEST_CASE("[GDSL] Typecheck rejects an undeclared signal in emit (typo_signal)") {
	const gdsl::Program prog = parse(
			"type P @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule R by P:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 1, emit(hitt)\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
	// is_godot_class 分支先过（CharacterBody2D 真实存在），失败必须来自 signal 未声明。
	CHECK(err.find("signal") != std::string::npos);
}

TEST_CASE("[GDSL] Reject target ref without a target clause") {
	const gdsl::Program prog = parse(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule OnHit by Player:\n"
			"    when target.hp > 0\n"
			"    then self.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Typecheck a rule with a valid target clause") {
	const gdsl::Program prog = parse(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"type Bullet @extends Area2D\n"
			"state:\n"
			"    damage: int = 1\n"
			"\n"
			"rule OnHit by Bullet target Player:\n"
			"    when target.hp > 0\n"
			"    then target.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	REQUIRE(typed.rules.size() == 1);
	CHECK(typed.rules[0].by == "Bullet");
	CHECK(typed.rules[0].target == "Player");
	CHECK(typed.rules[0].guard.ref == gdsl::RefOwner::Target);
	CHECK(typed.rules[0].guard.field == "hp");
	REQUIRE(typed.rules[0].effects.size() == 1);
	CHECK(typed.rules[0].effects[0].ref == gdsl::RefOwner::Target);
	CHECK(typed.rules[0].effects[0].field == "hp");
}

TEST_CASE("[GDSL] Reject rule with unknown target type") {
	const gdsl::Program prog = parse(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule OnHit by Player target Ghost:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject rule referencing unknown target field") {
	const gdsl::Program prog = parse(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"type Bullet @extends Area2D\n"
			"state:\n"
			"    damage: int = 1\n"
			"\n"
			"rule OnHit by Bullet target Player:\n"
			"    when target.mana > 0\n"
			"    then target.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Typecheck error names the offending symbol") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"\n"
			"rule OnHit by Ghost:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	// FR-009：报错必须点名违规符号（Ghost），不是空泛的 "unknown type"。
	CHECK(err.find("Ghost") != std::string::npos);
}

TEST_CASE("[GDSL] Reject float literal into int field (default)") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"state:\n"
			"    hp: int = 0.5\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject non-numeric default into int field") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"state:\n"
			"    hp: int = hello\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject float literal into int field (effect)") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule Tick by A:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 0.5\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject float literal into int field (guard)") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule Tick by A:\n"
			"    when self.hp > 0.5\n"
			"    then self.hp -= 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Accept int literal widening into float field") {
	const gdsl::Program prog = parse(
			"type A @extends Node2D\n"
			"state:\n"
			"    speed: float = 400\n"
			"    hp: int = 3\n"
			"\n"
			"rule Accel by A:\n"
			"    when self.hp > 0\n"
			"    then self.speed = 3\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	CHECK(typed.types[0].fields[0].type == gdsl::ValueType::Float);
}

TEST_CASE("[GDSL] Gap B: accept a real Godot base class") {
	const gdsl::Program prog = parse(
			"type Weapon @extends Resource\n"
			"\n"
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	REQUIRE(typed.types.size() == 2);
	CHECK(typed.types[1].base == "CharacterBody2D");
}

TEST_CASE("[GDSL] Gap B: reject hallucinated @extends base") {
	const gdsl::Program prog = parse(
			"type Player @extends FooBar\n"
			"state:\n"
			"    hp: int = 3\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
	CHECK(err.find("FooBar") != std::string::npos);
}

TEST_CASE("[GDSL] Gap B: reject a typo'd @extends base") {
	const gdsl::Program prog = parse(
			"type Player @extends Node2Dd\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
	CHECK(err.find("Node2Dd") != std::string::npos);
}

TEST_CASE("[GDSL] Gap B: reject @extends naming a GDSL type, not a Godot class") {
	const gdsl::Program prog = parse(
			"type Bullet @extends Area2D\n"
			"\n"
			"type Player @extends Bullet\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
	CHECK(err.find("Bullet") != std::string::npos);
}

TEST_CASE("[GDSL] FR-005: reject numeric literal as a string field default") {
	const gdsl::Program prog = parse(
			"type P @extends CharacterBody2D\n"
			"state:\n"
			"    name: string = 123\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] FR-005: reject int literal as a string field default") {
	const gdsl::Program prog = parse(
			"type P @extends CharacterBody2D\n"
			"state:\n"
			"    hp: string = 3\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] FR-005: accept a quoted string default") {
	const gdsl::Program prog = parse(
			"type P @extends CharacterBody2D\n"
			"state:\n"
			"    nickname: string = \"hero\"\n");
	gdsl::TypedProgram typed;
	std::string err;
	REQUIRE(gdsl::typecheck(prog, typed, err));
	REQUIRE(typed.types.size() == 1);
	REQUIRE(typed.types[0].fields.size() == 1);
	CHECK(typed.types[0].fields[0].type == gdsl::ValueType::String);
}

// FR-005 mutation guard (dup_rule_name): 同类型两个同名 rule 会注册两个同名方法，是真实正确性缺口，
// codegen 会在同一个 class 上重复注册规则方法。typecheck 必须拒收。
TEST_CASE("[GDSL] FR-005: reject duplicate rule name for the same owner (dup_rule_name)") {
	const gdsl::Program prog = parse(
			"type P @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule R by P:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 1\n"
			"rule R by P:\n"
			"    when self.hp < 5\n"
			"    then self.hp += 1\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
	// is_godot_class 分支先过（CharacterBody2D 真实存在），失败必须来自重复 rule 名。
	CHECK(err.find("duplicate") != std::string::npos);
}

// FR-005 mutation guard (int_overflow): 超出 int64 的字面量会被 codegen 原样发射成 C 整型字面量，
// 溢出后存储值错误（wraps 到别的数）。typecheck 必须做 int64 范围检查并拒收。
TEST_CASE("[GDSL] FR-005: reject int literal out of int64 range (int_overflow)") {
	const gdsl::Program prog = parse(
			"type P @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 99999999999999999999\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
	CHECK(err.find("int64") != std::string::npos);
}

// FR-005 mutation guard (guard_type_mix): float 字段与 int 字面量做比较是 int→float 隐式拓宽。
// 赋值/默认值允许拓宽（语义精确），但 guard 比较必须类型精确——浮点值对整型字面量做 == 是静默精度
// 语义风险（LLM_UNFRIENDLY #4 「无隐式转换」政策）。typecheck 必须拒收并提示写成 5.0。
TEST_CASE("[GDSL] FR-005: reject int literal widening against a float field in a guard (guard_type_mix)") {
	const gdsl::Program prog = parse(
			"type P @extends CharacterBody2D\n"
			"state:\n"
			"    spd: float = 1.5\n"
			"\n"
			"rule R by P:\n"
			"    when self.spd == 5\n"
			"    then self.spd += 1.0\n");
	gdsl::TypedProgram typed;
	std::string err;
	CHECK_FALSE(gdsl::typecheck(prog, typed, err));
	CHECK_FALSE(err.empty());
	CHECK(err.find("spd") != std::string::npos);
}
