// gdsl/abihash.h — GDExtension method-bind 兼容哈希（当前仅 Object::emit_signal）
// 权威源（逐条核对，2026-08-30）：
//   core/object/method_info.cpp:87-116   MethodInfo::get_compatibility_hash 算法
//   core/templates/hashfuncs.h:112-122   hash_murmur3_one_32（seed = HASH_MURMUR3_SEED = 0x7F07C65）
//   core/templates/hashfuncs.h:144-150   hash_fmix32
//   core/object/method_bind.cpp:35-45    MethodBind::get_hash 重建 MethodInfo（flags = get_hint_flags()）
//   core/object/object.cpp:1864-1870     emit_signal 的 MethodInfo：1 个 STRING_NAME 参数、vararg、无返回、无默认参数
//   core/variant/variant.h:96-124        Variant::Type 枚举：NIL=0 … STRING_NAME=21
//   core/object/method_bind.h:178        MethodBindVarArgBase::is_vararg() == true → flags 含 METHOD_FLAG_VARARG
//
// 注意：方法名不参与哈希（get_compatibility_hash 只哈希结构），所以 emit_signal 与
// call_deferred 同哈希（两者都是 vararg + 1 个 STRING_NAME 参数 + 无返回 + 无默认参数）。
#pragma once
#include <cstdint>

namespace gdsl {

// murmur3 单值（逐字移植 hashfuncs.h:112-122，p_seed 缺省 HASH_MURMUR3_SEED）。
inline uint32_t hash_murmur3_one_32(uint32_t p_in, uint32_t p_seed = 0x7F07C65u) {
	p_in *= 0xcc9e2d51u;
	p_in = (p_in << 15) | (p_in >> 17);
	p_in *= 0x1b873593u;

	p_seed ^= p_in;
	p_seed = (p_seed << 13) | (p_seed >> 19);
	p_seed = p_seed * 5u + 0xe6546b64u;

	return p_seed;
}

// fmix32（逐字移植 hashfuncs.h:144-150）。
inline uint32_t hash_fmix32(uint32_t h) {
	h ^= h >> 16;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;
	return h;
}

// Object::emit_signal 的兼容哈希。
// 对 emit_signal 的 MethodInfo 按 method_info.cpp:87-116 走一遍：
//   has_return=false → murmur(0)
//   arguments.size()=1 → murmur(1)
//   （has_return=false，无返回类型哈希）
//   arg[0].type=STRING_NAME(21) → murmur(21)
//   default_arguments.size()=0 → murmur(0)
//   const flag=0 → murmur(0)
//   vararg flag=1 → murmur(1)
//   最后 fmix32。
inline uint32_t emit_signal_compat_hash() {
	uint32_t hash = hash_murmur3_one_32(0u); // has_return == false
	hash = hash_murmur3_one_32(1u, hash); // arguments.size() == 1
	hash = hash_murmur3_one_32(21u, hash); // arg type STRING_NAME
	hash = hash_murmur3_one_32(0u, hash); // default_arguments.size() == 0
	hash = hash_murmur3_one_32(0u, hash); // const flag == 0
	hash = hash_murmur3_one_32(1u, hash); // vararg flag == 1
	return hash_fmix32(hash);
}

} // namespace gdsl
