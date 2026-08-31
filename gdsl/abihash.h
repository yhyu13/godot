// gdsl/abihash.h — GDExtension method-bind 兼容哈希（当前仅 Object::emit_signal）
// 权威源（逐条核对，2026-08-30，含真机 4.7-rc3 extension_api.json dump 对拍）：
//   core/object/method_info.cpp:86-116   MethodInfo::get_compatibility_hash 算法
//   core/templates/hashfuncs.h:112-122   hash_murmur3_one_32（seed = HASH_MURMUR3_SEED = 0x7F07C65）
//   core/templates/hashfuncs.h:144-150   hash_fmix32
//   core/object/method_bind.cpp:35-45    MethodBind::get_hash 重建 MethodInfo（flags = get_hint_flags()）
//   core/object/object.cpp:1866-1869     emit_signal 手写 MethodInfo（1 个 STRING_NAME 参数、vararg）
//   core/object/object.cpp:1151          Object::_emit_signal 返回 Error → 经 create_vararg_method_bind
//                                        从函数签名推导 return type = INT + class_name "Error"
//   core/variant/type_info.h:247-256     MAKE_ENUM_TYPE_INFO(Error)：VARIANT_TYPE=INT、class_name="Error"
//   core/variant/variant.h:96-124        Variant::Type 枚举：NIL=0 … INT=2 … STRING_NAME=21
//   core/string/ustring.cpp:2755-2764    String::hash = djb2（class_name 哈希用）
//
// 关键坑（2026-08-30 真机崩溃抓到）：emit_signal 的 MethodInfo **有返回值**（Error），
// 不是无返回。手写的 mi（object.cpp:1866）只填了 arguments 没填 return_val，但
// create_vararg_method_bind 会从 _emit_signal 的 C++ 签名（Error 返回）推导出
// return type = INT、class_name = "Error"（GetTypeInfo<Error>）。漏掉它 → 哈希错 →
// classdb_get_method_bind 返回 nullptr → 引擎崩溃「Method 'Object.emit_signal' has
// changed and no compatibility fallback」。
//
// 正确哈希 4047867050 (0xF1458CAA) 已用独立 Python 脚本按上述算法重算，并与
// 真机 `--dump-extension-api` 的 extension_api.json 对拍一致。
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

// djb2 字符串哈希（移植 ustring.cpp:2755-2764），用于 class_name 哈希。
inline uint32_t hash_djb2(const char *s) {
	uint32_t h = 5381u;
	while (*s) {
		h = ((h << 5) + h) + static_cast<uint32_t>(*s++);
	}
	return h;
}

// Object::emit_signal 的兼容哈希。
// 对 emit_signal 的真实 MethodInfo 按 method_info.cpp:86-116 走一遍：
//   has_return=true（Error 枚举）→ murmur(1)
//   arguments.size()=1 → murmur(1)
//   return type INT(2) → murmur(2)
//   return class_name "Error"（djb2 哈希）→ murmur(djb2("Error"))
//   arg[0].type=STRING_NAME(21) → murmur(21)
//   default_arguments.size()=0 → murmur(0)
//   const flag=0 → murmur(0)
//   vararg flag=1 → murmur(1)
//   最后 fmix32。
inline uint32_t emit_signal_compat_hash() {
	uint32_t hash = hash_murmur3_one_32(1u); // has_return == true (Error enum)
	hash = hash_murmur3_one_32(1u, hash); // arguments.size() == 1
	hash = hash_murmur3_one_32(2u, hash); // return type INT
	hash = hash_murmur3_one_32(hash_djb2("Error"), hash); // return class_name
	hash = hash_murmur3_one_32(21u, hash); // arg type STRING_NAME
	hash = hash_murmur3_one_32(0u, hash); // default_arguments.size() == 0
	hash = hash_murmur3_one_32(0u, hash); // const flag == 0
	hash = hash_murmur3_one_32(1u, hash); // vararg flag == 1
	return hash_fmix32(hash);
}

} // namespace gdsl
