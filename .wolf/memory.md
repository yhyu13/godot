# Memory

> Chronological action log. Hooks and AI append to this file automatically.
> Old sessions are consolidated by the daemon weekly.

| 00:01 | Wrote TDD+AI testing SOP covering metrics, test selection, writing guide, and AI follow-through | SOP_TDD_AI_TESTING.md | created | ~2000 |
| 10:09 | Researched Godot script↔C++ binding (3 routes) + LLM DSL prior art + GDScript/C#/native perf via 3 background agents; wrote DSL design doc | GODOT_LLM_DSL_DESIGN.md | created | ~1500 |
| 13:55 | Wrote impl plan + self-critique; resolved D1 (standalone gdsl/ dir + fast tests) and D2 (parser first) | PLAN_LLM_DSL_IMPL.md | created | ~700 |
| 14:05 | Built standalone MSVC+doctest harness; parser slice 1a red→green + 4 boundary tests green | gdsl/parser.{h,cpp}, gdsl/test_parser.cpp, gdsl/test.ps1 | green (4 passed / 9 asserts) | ~600 |
| 14:22 | parser slice 2: parse_state_field red→green + boundaries (9 cases / 23 asserts green) | gdsl/parser.cpp | green | ~400 |

## Session: 2026-08-29 21:37

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|

## Session: 2026-08-29 21:51

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|
| 21:54 | Edited doc_ai/PLAN_LLM_DSL_IMPL.md | — | ~653 |
| 22:00 | Edited gdsl/parser.h | — | ~126 |
| 22:00 | Edited gdsl/parser.cpp | — | ~64 |
| 22:01 | Edited gdsl/test_parser.cpp | — | ~92 |
| 22:01 | Edited gdsl/test.ps1 | — | ~142 |
| 22:01 | Edited gdsl/test.ps1 | — | ~39 |
| 22:02 | Edited gdsl/parser.cpp | — | ~362 |
| 22:03 | Edited gdsl/test_parser.cpp | — | ~200 |
| 22:04 | Edited JOURNEY.md | — | ~346 |
| 22:05 | Session end: 9 writes across 6 files (PLAN_LLM_DSL_IMPL.md, parser.h, parser.cpp, test_parser.cpp, test.ps1) | 1 reads | ~3149 tok |
| 22:13 | Session end: 9 writes across 6 files (PLAN_LLM_DSL_IMPL.md, parser.h, parser.cpp, test_parser.cpp, test.ps1) | 1 reads | ~3149 tok |
| 22:21 | Edited JOURNEY.md | — | ~99 |
| 22:22 | Edited gdsl/parser.h | — | ~145 |
| 22:22 | Edited gdsl/parser.cpp | — | ~22 |
| 22:22 | Edited gdsl/parser.cpp | — | ~40 |
| 22:22 | Edited gdsl/parser.cpp | — | ~84 |
| 22:22 | Edited gdsl/test_parser.cpp | — | ~123 |
| 22:22 | Edited gdsl/parser.cpp | — | ~211 |
| 22:23 | Edited gdsl/test_parser.cpp | — | ~257 |
| 22:23 | Edited JOURNEY.md | — | ~127 |
| 22:24 | Session end: 18 writes across 6 files (PLAN_LLM_DSL_IMPL.md, parser.h, parser.cpp, test_parser.cpp, test.ps1) | 2 reads | ~4805 tok |
| 22:40 | Edited JOURNEY.md | — | ~42 |
| 22:40 | Edited JOURNEY.md | — | ~74 |
| 22:41 | Edited gdsl/.gitignore | — | ~9 |
| 22:42 | Session end: 21 writes across 7 files (PLAN_LLM_DSL_IMPL.md, parser.h, parser.cpp, test_parser.cpp, test.ps1) | 2 reads | ~4329 tok |

## Session: 2026-08-30 09:43

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|
| (backfill) | Era 10 全切片未入账：S3 parser 补全 + S1 emit_tscn + S2 JSON/schema + S4 typecheck + S5 emit_c（35 用例 125 断言全绿，commit 15a6541） | gdsl/ 全 13 文件 | green | ~3000 |
| 09:43 | 状态核对：repo 已到 Era 10（35/125 绿，S1–S5 全红绿）；发现本 memory.md 滞后于 JOURNEY/STATUS | — | 核对完成 | ~800 |
| (now) | S6.1：emit_c 扩到真 GDExtension（classdb_register_extension_class6 + create/free instance + entry point）red→green | gdsl/codegen_logic.{h,cpp}, gdsl/test_codegen.cpp | green（38 用例 / 147 断言） | ~3000 |
| 13:12 | S6.2a：effect/ptrcall codegen v1——gdsl/effect.{h,cpp}（guard/effect 解析器）+ typecheck 结构化 when/then + emit_c 规则→方法（_impl/_call/_ptr + classdb_register_extension_class_method）red→green；D4 ontology v1=set_field；生成 C 经 stub 头 cl /c 语法编译通过 | gdsl/effect.{h,cpp}, gdsl/typecheck.{h,cpp}, gdsl/codegen_logic.cpp, gdsl/test_effect.cpp, gdsl/test_typecheck.cpp, gdsl/test_codegen.cpp, gdsl/test.ps1 | green（57 用例 / 226 断言） | ~3000 |
| 15:15 | Edited C:/Users/XINDONG/AppData/Local/Temp/kilo/gdsl_repro.cpp | — | ~224 |
| 15:16 | Session end: 1 writes across 1 files (gdsl_repro.cpp) | 19 reads | ~20434 tok |
| (now) | S6.2b：emit_signal codegen——D5 哈希从 Godot 源码移植 murmur3/fmix 算出 2866548813（gdsl/abihash.h + 独立程序 + test 三处一致）；effect 增 EffectKind::Emit + parse_emit（零参）；codegen 走 classdb_get_method_bind + object_method_bind_call + get_variant_from_type_constructor(STRING_NAME) Variant 封送 + static StringName 缓存；生成 C 经 stub 头 cl /c 通过 | gdsl/abihash.h, gdsl/effect.{h,cpp}, gdsl/typecheck.cpp, gdsl/codegen_logic.cpp, gdsl/test_abihash.cpp, gdsl/test_effect.cpp, gdsl/test_typecheck.cpp, gdsl/test_codegen.cpp, gdsl/test.ps1 | green（66 用例 / 264 断言） | ~3000 |

## Session: 2026-08-30 15:50

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|
| 15:59 | Edited .kilo/command/git-review.md | — | ~393 |
| 15:59 | Edited kilo.json | — | ~299 |
| 15:59 | Edited .kilo/review-warnings.md | — | ~40 |
| 16:00 | Edited .kilo/command/git-review.md | — | ~29 |
| 16:00 | Edited .kilo/command/git-review.md | — | ~30 |
| 16:01 | Edited .kilo/command/git-review.md | — | ~30 |
| 16:13 | Session end: 6 writes across 3 files (git-review.md, kilo.json, review-warnings.md) | 7 reads | ~2747 tok |

## Session: 2026-08-30 16:20

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|
| 16:20 | S6.2c：跨参与者 target codegen——D6（target 显式子句 `rule ... by <Owner> target <Type>:`）+ D7（ABI 无 object_get_instance，instance binding 取回）；parser/effect/typecheck/codegen 四层 red→green；生成 C 经扩展 stub 头 cl /c SYNTAX OK | gdsl/parser.{h,cpp}, gdsl/effect.{h,cpp}, gdsl/typecheck.{h,cpp}, gdsl/codegen_logic.cpp, gdsl/test_parser.cpp, gdsl/test_effect.cpp, gdsl/test_typecheck.cpp, gdsl/test_codegen.cpp | green（77 用例 / 319 断言） | ~4000 |
| 16:24 | Session end: 6 writes across 3 files (git-review.md, kilo.json, review-warnings.md) | 7 reads | ~2747 tok |

## Session: 2026-08-30 16:27

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|

## Session: 2026-08-30 17:08

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|

## Session: 2026-08-30 19:19

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|
| 21:12 | SDD init：修复 OpenWolf specs 运行时（dist/hooks/specs→.wolf/specs，pre-read/pre-write 解决 MODULE_NOT_FOUND）；promote SOP §4.4 DoD 为硬门（/implement 4 目录 + post-test.js 红测阻塞）；写 9 spec（8 retro + 1 forward S6）；active=009；77 用例 319 断言绿 | .wolf/specs/, .wolf/hooks/post-test.js, .claude/.codex/.opencode/.kilo commands/implement.md, specs/001-009, .wolf/STATUS.md | green | ~4000 |

## Session: 2026-08-31 (S6 真机集成)

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|
| — | route B 真机跑通（.gdsl→C→cl 编 .dll→.gdextension→引擎加载，引擎二进制在 .godot-bin/）；修两个真机 bug：① emit_signal 哈希 2866548813→4047867050（根因 Error 返回值，真机 --dump-extension-api 对拍）② hint_string NULL→gdsl_empty_string[8]（property_info.h:168 无条件解引用） | gdsl/abihash.h, gdsl/codegen_logic.cpp, gdsl/test_abihash.cpp, gdsl/toolchain/, gdsl/example/ | 83 用例 / 335 断言绿 | ~4000 |
| — | 未修：退出阶段 segfault（EXIT 139）——任何注册方法（有 rule）的 GDExtension 退出都崩，最小 self-only 也崩；假设 GDExtensionMethodBind 存 DLL 函数指针 + DLL 先于 ClassDB::cleanup 卸载；证据缺口（cdb 卡住/procdump+WER 无 dump），无符号栈 | gdsl/example/DIAGNOSTIC.txt, combo_trace.c | 未修（根因未证实） | ~4000 |
| — | 纠错：还原信号名 StringName p_is_static false→true（对拍 string_name.cpp 证伪「复用字面量缓冲区」注释，p_static 只影响 static_count；且 segfault 无 emit 也复现）；恢复单测全绿 | gdsl/codegen_logic.cpp, gdsl/test_codegen.cpp | 83/335 绿 | ~800 |
| — | 更新 JOURNEY（Era 17）+ STATUS + cerebrum + buglog + memory；gdsl/.gitignore 排除 .godot/、*.gdextension.uid、DIAGNOSTIC.txt、*_trace.c | JOURNEY.md, .wolf/* | 已落盘 | ~1500 |

## Session: 2026-08-31 12:32

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|

## Session: 2026-09-01 (Gap A)

| Time | Action | File(s) | Outcome | ~Tokens |
|------|--------|---------|---------|--------|
| — | Gap A：typecheck 字面量/类型 coercion——classify_literal + literal_matches，三处落闸（state default / guard.value / effect.value），find_field 取代 has_field；+5 红绿用例 | gdsl/typecheck.cpp, gdsl/test_typecheck.cpp | green（88 用例 / 350 断言） | ~2500 |
