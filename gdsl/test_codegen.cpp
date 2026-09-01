#include "doctest.h"
#include "parser.h"
#include "typecheck.h"
#include "codegen_logic.h"

static gdsl::TypedProgram typed_program(const char *src) {
	gdsl::Program p;
	std::string err;
	REQUIRE(gdsl::parse_program(src, p, err));
	gdsl::TypedProgram typed;
	REQUIRE(gdsl::typecheck(p, typed, err));
	return typed;
}

TEST_CASE("[GDSL] Emit GDExtension entry point") {
	const gdsl::TypedProgram typed = typed_program("type Player @extends Node2D\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("#include <gdextension_interface.h>") != std::string::npos);
	CHECK(c.find("GDExtensionBool GDE_EXPORT gdsl_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)") != std::string::npos);
	CHECK(c.find("r_initialization->minimum_initialization_level = GDEXTENSION_INITIALIZATION_SCENE;") != std::string::npos);
}

TEST_CASE("[GDSL] Emit class registration via classdb_register_extension_class6") {
	const gdsl::TypedProgram typed = typed_program("type Player @extends CharacterBody2D\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("GDExtensionClassCreationInfo6 class_info = { 0 };") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&class_name, \"Player\", false);") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&parent_name, \"CharacterBody2D\", false);") != std::string::npos);
	CHECK(c.find("gdsl_classdb_register_extension_class6(gdsl_library, &class_name, &parent_name, &class_info);") != std::string::npos);
}

TEST_CASE("[GDSL] Emit create/free instance lifecycle") {
	const gdsl::TypedProgram typed = typed_program("type Player @extends Node2D\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("static GDExtensionObjectPtr player_create_instance(void *p_class_userdata, GDExtensionBool p_notify_postinitialize)") != std::string::npos);
	CHECK(c.find("gdsl_classdb_construct_object(&parent_name);") != std::string::npos);
	CHECK(c.find("gdsl_mem_alloc(sizeof(Player));") != std::string::npos);
	CHECK(c.find("gdsl_object_set_instance(object, &class_name, (GDExtensionClassInstancePtr)self);") != std::string::npos);
	CHECK(c.find("static void player_free_instance(void *p_class_userdata, GDExtensionClassInstancePtr p_instance)") != std::string::npos);
	CHECK(c.find("gdsl_mem_free(self);") != std::string::npos);
}

TEST_CASE("[GDSL] Emit state struct with typed fields and defaults") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"    speed: float = 400.0\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("int64_t hp;") != std::string::npos);
	CHECK(c.find("double speed;") != std::string::npos);
	CHECK(c.find("self->hp = 3;") != std::string::npos);
	CHECK(c.find("self->speed = 400.0;") != std::string::npos);
}

TEST_CASE("[GDSL] Emit named field as pointer") {
	const gdsl::TypedProgram typed = typed_program(
			"type Weapon @extends Resource\n"
			"\n"
			"type Player @extends Node2D\n"
			"state:\n"
			"    weapon: Weapon = null\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("Weapon * weapon;") != std::string::npos);
	CHECK(c.find("self->weapon = NULL;") != std::string::npos);
}

TEST_CASE("[GDSL] Emit rule as registered method with guard and set_field") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule TakeDamage by Player:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 1\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("gdsl_classdb_register_extension_class_method(gdsl_library, &class_name, &method_info);") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&method_name, \"take_damage\", false);") != std::string::npos);
	CHECK(c.find("method_info.call_func = player_take_damage_call;") != std::string::npos);
	CHECK(c.find("method_info.ptrcall_func = player_take_damage_ptr;") != std::string::npos);
	CHECK(c.find("method_info.method_flags = GDEXTENSION_METHOD_FLAG_NORMAL;") != std::string::npos);
	CHECK(c.find("static void player_take_damage_impl(Player *self) {") != std::string::npos);
	CHECK(c.find("if (!(self->hp > 0)) {") != std::string::npos);
	CHECK(c.find("self->hp -= 1;") != std::string::npos);
}

TEST_CASE("[GDSL] Emit add and assign effects to correct C operators") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"    speed: float = 400.0\n"
			"\n"
			"rule Buff by Player:\n"
			"    when self.hp >= 0\n"
			"    then self.hp += 2, self.speed = 0\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("if (!(self->hp >= 0)) {") != std::string::npos);
	CHECK(c.find("self->hp += 2;") != std::string::npos);
	CHECK(c.find("self->speed = 0;") != std::string::npos);
}

TEST_CASE("[GDSL] Emit no method registration for ruleless types") {
	const gdsl::TypedProgram typed = typed_program("type Player @extends Node2D\n");
	const std::string c = gdsl::emit_c(typed);
	CHECK(c.find("gdsl_classdb_register_extension_class_method(gdsl_library, &class_name, &method_info);") == std::string::npos);
}

TEST_CASE("[GDSL] Emit GDExtension source is deterministic with a rule") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule TakeDamage by Player:\n"
			"    when self.hp > 0\n"
			"    then self.hp -= 1\n");
	const std::string a = gdsl::emit_c(typed);
	const std::string b = gdsl::emit_c(typed);
	CHECK_FALSE(a.empty());
	CHECK(a == b);
}

TEST_CASE("[GDSL] Emit GDExtension source is deterministic") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n");
	const std::string a = gdsl::emit_c(typed);
	const std::string b = gdsl::emit_c(typed);
	CHECK_FALSE(a.empty());
	CHECK(a == b);
}

TEST_CASE("[GDSL] Emit emit_signal codegen with method-bind hash and helper") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends CharacterBody2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule Die by Player:\n"
			"    when self.hp <= 0\n"
			"    then emit(died)\n");
	const std::string c = gdsl::emit_c(typed);
	// 缓存 Object::emit_signal MethodBind（精确兼容哈希，来自 abihash.h）。
	CHECK(c.find("gdsl_classdb_get_method_bind(&sn_Object, &sn_emit_signal, 4047867050);") != std::string::npos);
	// 信号名 static StringName 缓存 + 发射 helper 调用。
	CHECK(c.find("static gdsl_StringName gdsl_signal_died;") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&gdsl_signal_died, \"died\", true);") != std::string::npos);
	CHECK(c.find("gdsl_emit_signal(self->object, &gdsl_signal_died);") != std::string::npos);
	// Variant 封送：从 raw StringName 构造 STRING_NAME Variant + object_method_bind_call。
	CHECK(c.find("gdsl_get_variant_from_type_constructor(GDEXTENSION_VARIANT_TYPE_STRING_NAME)") != std::string::npos);
	CHECK(c.find("gdsl_object_method_bind_call(gdsl_emit_signal_mb, p_object, args, 1,") != std::string::npos);
	// 缓存加载。
	CHECK(c.find("(GDExtensionInterfaceClassdbGetMethodBind)p_get_proc_address(\"classdb_get_method_bind\")") != std::string::npos);
	CHECK(c.find("(GDExtensionInterfaceGetVariantFromTypeConstructor)p_get_proc_address(\"get_variant_from_type_constructor\")") != std::string::npos);
}

TEST_CASE("[GDSL] Emit emit_signal de-duplicates repeated signal names") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule A by Player:\n"
			"    when self.hp > 0\n"
			"    then emit(died)\n"
			"\n"
			"rule B by Player:\n"
			"    when self.hp <= 0\n"
			"    then emit(died)\n");
	const std::string c = gdsl::emit_c(typed);
	// 同一信号名只声明一次缓存。
	const size_t first = c.find("static gdsl_StringName gdsl_signal_died;");
	REQUIRE(first != std::string::npos);
	CHECK(c.find("static gdsl_StringName gdsl_signal_died;", first + 1) == std::string::npos);
}

TEST_CASE("[GDSL] Emit emit_signal codegen is deterministic") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule Die by Player:\n"
			"    when self.hp <= 0\n"
			"    then emit(died)\n");
	CHECK(gdsl::emit_c(typed) == gdsl::emit_c(typed));
}

TEST_CASE("[GDSL] Emit cross-participant target codegen") {
	const gdsl::TypedProgram typed = typed_program(
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
	const std::string c = gdsl::emit_c(typed);
	// 方法带 target 参数 + 经 instance binding 取 target 的 C struct。
	CHECK(c.find("static void bullet_on_hit_impl(Bullet *self, GDExtensionObjectPtr p_target) {") != std::string::npos);
	CHECK(c.find("Player *target = (Player *)gdsl_object_get_instance_binding(p_target, gdsl_library, &gdsl_binding_callbacks);") != std::string::npos);
	CHECK(c.find("if (target == NULL) {") != std::string::npos);
	CHECK(c.find("if (!(target->hp > 0)) {") != std::string::npos);
	CHECK(c.find("target->hp -= 1;") != std::string::npos);
	// 绑定设置 + API 缓存加载。
	CHECK(c.find("gdsl_object_set_instance_binding(object, gdsl_library, self, &gdsl_binding_callbacks);") != std::string::npos);
	CHECK(c.find("(GDExtensionInterfaceObjectGetInstanceBinding)p_get_proc_address(\"object_get_instance_binding\")") != std::string::npos);
	CHECK(c.find("(GDExtensionInterfaceObjectSetInstanceBinding)p_get_proc_address(\"object_set_instance_binding\")") != std::string::npos);
	CHECK(c.find("(GDExtensionInterfaceGetVariantToTypeConstructor)p_get_proc_address(\"get_variant_to_type_constructor\")") != std::string::npos);
	// 方法注册：argument_count = 1 + OBJECT 参数信息。
	CHECK(c.find("method_info.argument_count = 1;") != std::string::npos);
	CHECK(c.find("arg_info[0].type = GDEXTENSION_VARIANT_TYPE_OBJECT;") != std::string::npos);
}

TEST_CASE("[GDSL] Emit cross-participant target codegen is deterministic") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"type Bullet @extends Area2D\n"
			"\n"
			"rule OnHit by Bullet target Player:\n"
			"    when target.hp > 0\n"
			"    then target.hp -= 1\n");
	CHECK(gdsl::emit_c(typed) == gdsl::emit_c(typed));
}

TEST_CASE("[GDSL] Emit schema-safe setter (type-check before marshal)") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n");
	const std::string c = gdsl::emit_c(typed);
	// 标量 setter 在封送前检查 Variant 类型：LLM 改字段类型后，旧状态类型不匹配就跳过，不污染默认值。
	CHECK(c.find("if (gdsl_variant_get_type((GDExtensionConstVariantPtr)p_args[0]) != GDEXTENSION_VARIANT_TYPE_INT) {") != std::string::npos);
}

TEST_CASE("[GDSL] String field emits char* member and no double-quoted default") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    name: string = \"hero\"\n");
	const std::string c = gdsl::emit_c(typed);
	// 修复双层引号 bug：default_value 已含引号，codegen 不再包一层。
	CHECK(c.find("\"\"hero\"\"") == std::string::npos);
	CHECK(c.find("char * name;") != std::string::npos);
	// constructor 用 gdsl_mem_alloc 拷贝默认值（不用只读字面量，避免 setter/free 撞字面量）。
	CHECK(c.find("self->name = (char *)gdsl_mem_alloc(sizeof(\"hero\"));") != std::string::npos);
	CHECK(c.find("memcpy(self->name, \"hero\", sizeof(\"hero\"));") != std::string::npos);
}

TEST_CASE("[GDSL] String field emits getter/setter/property (hot-reload preserve)") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    name: string = \"hero\"\n");
	const std::string c = gdsl::emit_c(typed);
	// getter 经 string_new_with_utf8_chars 建 transient String，再包成 STRING Variant。
	CHECK(c.find("gdsl_string_new((GDExtensionUninitializedStringPtr)&tmp, self->name ? self->name : \"\");") != std::string::npos);
	CHECK(c.find("gdsl_get_variant_from_type_constructor(GDEXTENSION_VARIANT_TYPE_STRING)") != std::string::npos);
	// schema-safe setter：类型不匹配跳过。
	CHECK(c.find("if (gdsl_variant_get_type((GDExtensionConstVariantPtr)p_args[0]) != GDEXTENSION_VARIANT_TYPE_STRING) {") != std::string::npos);
	// setter 经 string_to_utf8_chars 取值 + gdsl_mem_alloc 新 buffer + free 旧 buffer。
	CHECK(c.find("GDExtensionInt len = gdsl_string_to_utf8_chars((GDExtensionConstStringPtr)&tmp, NULL, 0);") != std::string::npos);
	CHECK(c.find("char *buf = (char *)gdsl_mem_alloc((size_t)len + 1);") != std::string::npos);
	CHECK(c.find("if (self->name) gdsl_mem_free(self->name);") != std::string::npos);
	// destructor 释放字段 buffer（干净 ownership）。
	CHECK(c.find("if (self->name) gdsl_mem_free(self->name);") != std::string::npos);
	// property 注册类型 = STRING + get_/set_ 方法名。
	CHECK(c.find("pi.type = GDEXTENSION_VARIANT_TYPE_STRING;") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&pn, \"name\", false);") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&sn, \"set_name\", false);") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&gn, \"get_name\", false);") != std::string::npos);
	// 新增 API 缓存：string_new_with_utf8_chars / string_to_utf8_chars。
	CHECK(c.find("(GDExtensionInterfaceStringNewWithUtf8Chars)p_get_proc_address(\"string_new_with_utf8_chars\")") != std::string::npos);
	CHECK(c.find("(GDExtensionInterfaceStringToUtf8Chars)p_get_proc_address(\"string_to_utf8_chars\")") != std::string::npos);
}

TEST_CASE("[GDSL] Signal-name StringName is static-once (no per-emit creation, no unbounded leak)") {
	const gdsl::TypedProgram typed = typed_program(
			"type Player @extends Node2D\n"
			"state:\n"
			"    hp: int = 3\n"
			"\n"
			"rule Die by Player:\n"
			"    when self.hp <= 0\n"
			"    then emit(died)\n");
	const std::string c = gdsl::emit_c(typed);
	// 信号名 StringName 只在 init 创建一次、p_is_static=true（static StringName 免析构、程序周期复用）。
	CHECK(c.find("static gdsl_StringName gdsl_signal_died;") != std::string::npos);
	CHECK(c.find("gdsl_string_name_new(&gdsl_signal_died, \"died\", true);") != std::string::npos);
	// helper 只引用缓存，不新建（无 per-emit StringName 创建）。
	CHECK(c.find("gdsl_emit_signal(self->object, &gdsl_signal_died);") != std::string::npos);
	// 无 string_destroy/string_name_destroy 被调用（ABI 没有这个函数）。
	CHECK(c.find("string_destroy") == std::string::npos);
	CHECK(c.find("string_name_destroy") == std::string::npos);
}

