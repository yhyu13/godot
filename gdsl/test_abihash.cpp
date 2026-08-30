#include "doctest.h"
#include "abihash.h"

// 把 emit_signal 兼容哈希钉在独立计算的黄金值上（见 doc_ai/PLAN_LLM_DSL_IMPL.md D5）。
// 黄金值 2866548813 (0xAADC104D) 由独立 standalone 程序按 Godot 源码算法（method_info.cpp:87-116
// + hashfuncs.h:112-122,144-150 + object.cpp:1864-1870 + variant.h:96-124）计算得到。
// 若引擎侧 MethodInfo 结构改变导致哈希漂移，此测试会红，提醒重新核对 extension_api.json dump。
TEST_CASE("[GDSL] emit_signal compatibility hash matches golden value") {
	CHECK(gdsl::emit_signal_compat_hash() == 2866548813u);
	CHECK(gdsl::emit_signal_compat_hash() == 0xAADC104Du);
}

// murmur3 移植的属性检查：fmix32(0) == 0（每步都是 0 异或/乘 0，可手推），
// 以及 one_32 是确定的（同输入同输出）。防止移植时手滑。
TEST_CASE("[GDSL] murmur3 port determinism and fmix32(0)==0") {
	CHECK(gdsl::hash_fmix32(0u) == 0u);
	CHECK(gdsl::hash_murmur3_one_32(21u) == gdsl::hash_murmur3_one_32(21u));
}
