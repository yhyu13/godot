#include "doctest.h"
#include "effect.h"

TEST_CASE("[GDSL] Parse guard self field comparison") {
	gdsl::Guard g;
	std::string err;
	REQUIRE(gdsl::parse_guard("self.hp > 0", g, err));
	CHECK(g.field == "hp");
	CHECK(g.cmp == ">");
	CHECK(g.value == "0");
}

TEST_CASE("[GDSL] Parse guard bare field comparison") {
	gdsl::Guard g;
	std::string err;
	REQUIRE(gdsl::parse_guard("hp <= 3", g, err));
	CHECK(g.field == "hp");
	CHECK(g.cmp == "<=");
	CHECK(g.value == "3");
}

TEST_CASE("[GDSL] Parse guard float value and >= operator") {
	gdsl::Guard g;
	std::string err;
	REQUIRE(gdsl::parse_guard("self.speed >= 0.5", g, err));
	CHECK(g.field == "speed");
	CHECK(g.cmp == ">=");
	CHECK(g.value == "0.5");
}

TEST_CASE("[GDSL] Parse target guard field comparison") {
	gdsl::Guard g;
	std::string err;
	REQUIRE(gdsl::parse_guard("target.hp > 0", g, err));
	CHECK(g.ref == gdsl::RefOwner::Target);
	CHECK(g.field == "hp");
	CHECK(g.cmp == ">");
	CHECK(g.value == "0");
}

TEST_CASE("[GDSL] Reject unknown guard ref owner") {
	gdsl::Guard g;
	std::string err;
	CHECK_FALSE(gdsl::parse_guard("other.hp > 0", g, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject guard missing comparison operator") {
	gdsl::Guard g;
	std::string err;
	CHECK_FALSE(gdsl::parse_guard("self.hp", g, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject non-numeric guard value") {
	gdsl::Guard g;
	std::string err;
	CHECK_FALSE(gdsl::parse_guard("self.hp > foo", g, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Parse set_field effects with subtract and assign") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	REQUIRE(gdsl::parse_effects("self.hp -= 1, self.speed = 0", fx, err));
	REQUIRE(fx.size() == 2);
	CHECK(fx[0].kind == gdsl::EffectKind::Sub);
	CHECK(fx[0].field == "hp");
	CHECK(fx[0].value == "1");
	CHECK(fx[1].kind == gdsl::EffectKind::Set);
	CHECK(fx[1].field == "speed");
	CHECK(fx[1].value == "0");
}

TEST_CASE("[GDSL] Parse add effect") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	REQUIRE(gdsl::parse_effects("self.hp += 2", fx, err));
	REQUIRE(fx.size() == 1);
	CHECK(fx[0].kind == gdsl::EffectKind::Add);
	CHECK(fx[0].field == "hp");
	CHECK(fx[0].value == "2");
}

TEST_CASE("[GDSL] Parse bare field effect") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	REQUIRE(gdsl::parse_effects("hp = 0", fx, err));
	REQUIRE(fx.size() == 1);
	CHECK(fx[0].kind == gdsl::EffectKind::Set);
	CHECK(fx[0].field == "hp");
}

TEST_CASE("[GDSL] Parse emit_signal effect") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	REQUIRE(gdsl::parse_effects("emit(hit)", fx, err));
	REQUIRE(fx.size() == 1);
	CHECK(fx[0].kind == gdsl::EffectKind::Emit);
	CHECK(fx[0].signal_name == "hit");
}

TEST_CASE("[GDSL] Parse emit mixed with set_field effects") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	REQUIRE(gdsl::parse_effects("self.hp -= 1, emit(hit)", fx, err));
	REQUIRE(fx.size() == 2);
	CHECK(fx[0].kind == gdsl::EffectKind::Sub);
	CHECK(fx[0].field == "hp");
	CHECK(fx[1].kind == gdsl::EffectKind::Emit);
	CHECK(fx[1].signal_name == "hit");
}

TEST_CASE("[GDSL] Reject emit with argument list (v1 zero-arg only)") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	CHECK_FALSE(gdsl::parse_effects("emit(hit, 3)", fx, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject emit with empty signal name") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	CHECK_FALSE(gdsl::parse_effects("emit()", fx, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject unterminated emit") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	CHECK_FALSE(gdsl::parse_effects("emit(hit", fx, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Parse target field effect") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	REQUIRE(gdsl::parse_effects("target.hp -= 1", fx, err));
	REQUIRE(fx.size() == 1);
	CHECK(fx[0].kind == gdsl::EffectKind::Sub);
	CHECK(fx[0].ref == gdsl::RefOwner::Target);
	CHECK(fx[0].field == "hp");
	CHECK(fx[0].value == "1");
}

TEST_CASE("[GDSL] Reject unknown effect ref owner") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	CHECK_FALSE(gdsl::parse_effects("other.hp -= 1", fx, err));
	CHECK_FALSE(err.empty());
}

TEST_CASE("[GDSL] Reject effect missing assignment") {
	std::vector<gdsl::Effect> fx;
	std::string err;
	CHECK_FALSE(gdsl::parse_effects("self.hp", fx, err));
	CHECK_FALSE(err.empty());
}
