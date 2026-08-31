#include "doctest.h"
#include "abihash.h"

// 把 emit_signal 兼容哈希钉在真机 dump 对拍值上（见 abihash.h 头注释与
// doc_ai/PLAN_LLM_DSL_IMPL.md D5）。
// 黄金值 4047867050 (0xF1458CAA) 来自真机 `Godot 4.7-rc3 --dump-extension-api`
// 的 extension_api.json「Object.emit_signal.hash」，并与独立脚本按 Godot 源码算法
// （method_info.cpp:86-116 + hashfuncs.h + type_info.h + ustring.cpp djb2）重算一致。
// 若引擎侧 MethodInfo 结构改变导致哈希漂移，此测试会红，提醒重新对拍 dump。
TEST_CASE("[GDSL] emit_signal compatibility hash matches golden value") {
	CHECK(gdsl::emit_signal_compat_hash() == 4047867050u);
	CHECK(gdsl::emit_signal_compat_hash() == 0xF1458CAAu);
}

// murmur3 移植的属性检查：fmix32(0) == 0（每步都是 0 异或/乘 0，可手推），
// 以及 one_32 是确定的（同输入同输出）。防止移植时手滑。
TEST_CASE("[GDSL] murmur3 port determinism and fmix32(0)==0") {
	CHECK(gdsl::hash_fmix32(0u) == 0u);
	CHECK(gdsl::hash_murmur3_one_32(21u) == gdsl::hash_murmur3_one_32(21u));
}

// djb2 移植：标准 djb2("Error") 已知值，防止 hash_djb2 手滑。
TEST_CASE("[GDSL] djb2 port matches known value") {
	CHECK(gdsl::hash_djb2("Error") == 220205519u);
	CHECK(gdsl::hash_djb2("") == 5381u);
}
