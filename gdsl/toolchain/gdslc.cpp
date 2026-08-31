// gdsl/toolchain/gdslc.cpp — 声明式/逻辑层编译器 CLI：.gdsl → C 源，或 JSON → .tscn
// 用法：
//   gdslc logic <in.gdsl> <out.c>    逻辑层：.gdsl 文本 → GDExtension C 源
//   gdslc scene <in.json> <out.tscn> 声明式层：JSON 场景配方 → .tscn 文本
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "../parser.h"
#include "../typecheck.h"
#include "../codegen_logic.h"
#include "../scene_json.h"
#include "../codegen_declarative.h"

static std::string read_file(const std::string &path) {
	std::ifstream in(path, std::ios::binary);
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static bool write_file(const std::string &path, const std::string &content) {
	std::ofstream out(path, std::ios::binary);
	if (!out) {
		return false;
	}
	out << content;
	return true;
}

int main(int argc, char **argv) {
	if (argc != 4) {
		std::cerr << "usage: gdslc <logic|scene> <in> <out>\n";
		return 2;
	}
	const std::string mode = argv[1];
	const std::string in_path = argv[2];
	const std::string out_path = argv[3];
	const std::string src = read_file(in_path);

	if (mode == "scene") {
		gdsl::SceneSpec spec;
		std::string err;
		if (!gdsl::scene_from_json(src, spec, err)) {
			std::cerr << "scene error: " << err << "\n";
			return 1;
		}
		if (!write_file(out_path, gdsl::emit_tscn(spec))) {
			std::cerr << "cannot write " << out_path << "\n";
			return 1;
		}
		std::cout << "emitted " << out_path << "\n";
		return 0;
	}

	if (mode == "logic") {
		gdsl::Program prog;
		std::string err;
		if (!gdsl::parse_program(src, prog, err)) {
			std::cerr << "parse error: " << err << "\n";
			return 1;
		}
		gdsl::TypedProgram typed;
		if (!gdsl::typecheck(prog, typed, err)) {
			std::cerr << "typecheck error: " << err << "\n";
			return 1;
		}
		if (!write_file(out_path, gdsl::emit_c(typed))) {
			std::cerr << "cannot write " << out_path << "\n";
			return 1;
		}
		std::cout << "emitted " << out_path << "\n";
		return 0;
	}

	std::cerr << "unknown mode '" << mode << "'\n";
	return 2;
}
