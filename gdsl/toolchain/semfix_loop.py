#!/usr/bin/env python3
"""语义闭环证明：一个「编译有效但语义写反」的配方，被金标准抓住 → 把报错喂给 LLM → LLM 改对 → 全过。

这就是 FR-011 真正缺的那半：前面只跑到「编译 + playtest(读配方 effect)」，那半抓不到「写反」。
这脚本用 golden（对着正确值断言）去抓「+=1 该是 -=1」，再把 FAIL 喂回 LLM 看它能不能改对。

用法（仓库根）：
    python3 gdsl/toolchain/semfix_loop.py
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import golden as gold
import llm_conv_bench as llm
import playtest as pt


def run_golden(recipe, task):
    with tempfile.TemporaryDirectory(prefix="gdslsemfix_") as wd:
        gdsl = os.path.join(wd, "sf.gdsl")
        open(gdsl, "w", encoding="utf-8").write(recipe)
        dll, err = pt.build_dll(gdsl, wd)
        if err:
            return "BUILD FAIL: " + err[:200]
        open(os.path.join(wd, "sf.gdextension"), "w").write(
            '[configuration]\n\nentry_symbol = "gdsl_library_init"\ncompatibility_minimum = "4.7"\n'
            'reloadable = true\n\n[libraries]\n\nwindows.x86_64 = "res://sf.dll"\n')
        open(os.path.join(wd, "project.godot"), "w").write(
            'config_version=5\n\n[application]\n\nconfig/name="gdsl semfix"\n'
            'config/features=PackedStringArray("4.7")\n\n[autoload]\n\nPT="*res://playtest.gd"\n')
        open(os.path.join(wd, "playtest.gd"), "w").write(gold.gen_golden_playtest_gd(recipe, task))
        return pt.run_engine(wd)


def main():
    task = "basic_int"
    wrong = gold.KNOWINGLY_WRONG[task]
    print("=== 初始（故意写反：+=1 应为 -=1，编译有效但语义错）===")
    out = run_golden(wrong, task)
    fails = [l.strip() for l in out.splitlines() if "FAIL" in l]
    result = next((l.strip() for l in out.splitlines() if "RESULT" in l), "NO_RESULT")
    print("golden result:", result, "| fails:", fails)

    prompt = (
        "下面是一个 godot 玩法配方(gdsl)，它编译通过，但在引擎里跑语义 playtest 失败了。\n"
        "配方：\n```\n%s\n```\n"
        "playtest 失败信息：\n%s\n"
        "请修正这个配方，让它通过 playtest（语义正确）。只输出修正后的完整 .gdsl，不要解释。\n"
        % (wrong, "\n".join(fails))
    )
    print("\n=== 把失败喂给 LLM 让它改 ===")
    gen = llm.claude_generate(prompt)
    fixed = llm.extract_gdsl(gen["text"])
    print("LLM 修正后的配方：\n%s\n" % fixed[-400:])
    print("=== 修改后重跑 golden（应 ALLPASS）===")
    out2 = run_golden(fixed, task)
    result2 = next((l.strip() for l in out2.splitlines() if "RESULT" in l), "NO_RESULT")
    print("golden result after fix:", result2)
    print("\n>>> %s" % ("SEMFIX LOOP PASSED" if result2 == "RESULT ALLPASS" else "SEMFIX LOOP FAILED"))


if __name__ == "__main__":
    main()
