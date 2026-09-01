#!/usr/bin/env python3
"""FR-011 完整闭环：编译 + playtest 双阶段反馈，ScriptDoctor 式。

对每个任务：
  循环（≤ MAX_CYCLES）：
    1. 让 LLM 产出 .gdsl
    2. gdslc parse+typecheck → 编译无效？报错回喂 → 重来
    3. 编译有效 → 自动生成 playtest .gd（gdsl_playgen）→ 真引擎跑
    4. playtest 有 FAIL？把 FAIL 行回喂 → 重来
    5. RESULT ALLPASS → 收敛
  记 compile_cycles / playtest_cycles / 是否收敛。

用法：
    python3 gdsl/toolchain/loop_bench.py --tasks 2 --ks 0 --max-cycles 3 --json gdsl/loop_results.json
"""
import argparse
import json
import os
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import llm_conv_bench as llm
import gdsl_playgen as playgen
import playtest as pt

REPO = llm.REPO


def run_playtest(gdsl_text, stem):
    """gdsl_text -> playtest 输出（含 PASS/FAIL/RESULT 行）。返回 (output, err)."""
    with tempfile.TemporaryDirectory(prefix="gdslloop_") as wd:
        gdsl_path = os.path.join(wd, stem + ".gdsl")
        with open(gdsl_path, "w", encoding="utf-8") as f:
            f.write(gdsl_text)
        dll_path, err = pt.build_dll(gdsl_path, wd)
        if err:
            return None, err
        # 写 manifest + project + 自动生成的 playtest.gd
        with open(os.path.join(wd, stem + ".gdextension"), "w") as f:
            f.write("[configuration]\n\nentry_symbol = \"gdsl_library_init\"\ncompatibility_minimum = \"4.7\"\n"
                    "reloadable = true\n\n[libraries]\n\nwindows.x86_64 = \"res://%s.dll\"\n" % stem)
        with open(os.path.join(wd, "project.godot"), "w") as f:
            f.write('config_version=5\n\n[application]\n\nconfig/name="gdsl playtest"\n'
                    'config/features=PackedStringArray("4.7")\n\n[autoload]\n\nPT="*res://playtest.gd"\n')
        gd = playgen.gen_playtest_gd(gdsl_text)
        with open(os.path.join(wd, "playtest.gd"), "w") as f:
            f.write(gd)
        out = pt.run_engine(wd)
        return out, None


def converge(task, k, max_cycles):
    prompt = llm.build_prompt(task, k)
    log = []
    for cyc in range(1, max_cycles + 1):
        gen = llm.claude_generate(prompt)
        gsdl = llm.extract_gdsl(gen["text"])
        out, err = llm.validate(gsdl, tempfile.mkdtemp(prefix="gdslval_"))
        if not out:
            # 编译无效
            perr = err[:300]
            log.append({"cycle": cyc, "stage": "compile", "ok": False, "err": perr})
            prompt += "\n\n你上一次输出未通过编译校验：\n%s\n请修正并只重新输出完整 .gdsl。\n" % perr
            continue
        # 编译有效 → playtest
        pot, perr = run_playtest(gsdl, "loop")
        if pot is None:
            log.append({"cycle": cyc, "stage": "playtest", "ok": False, "err": perr[:300]})
            prompt += "\n\n你上一次配方编译通过但无法构建/运行：\n%s\n请修正并重新输出。\n" % perr
            continue
        allpass = "RESULT ALLPASS" in pot
        fails = [l for l in pot.splitlines() if l.strip().startswith("FAIL")]
        log.append({"cycle": cyc, "stage": "playtest", "ok": allpass,
                    "fails": fails[:3], "output": pot[-800:]})
        if allpass:
            return {"task": task["name"], "k": k, "converged": True, "cycles": cyc,
                    "log": log, "gsdl": gsdl, "cost": gen["cost"]}
        # playtest 失败 → 回喂 FAIL 行
        fb = ("\n".join(fails[:3]) if fails else "playtest 未通过（规则未按预期执行）")
        prompt += ("\n\n你上一次配方编译通过，但运行时 playtest 未通过：\n" + fb +
                   "\n请修正并只重新输出完整 .gdsl。\n")
    return {"task": task["name"], "k": k, "converged": False, "cycles": max_cycles,
            "log": log, "gsdl": None, "cost": sum(l for _, l in [])}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tasks", type=int, default=2)
    ap.add_argument("--ks", default="0")
    ap.add_argument("--max-cycles", type=int, default=3)
    ap.add_argument("--json", default=os.path.join(REPO, "gdsl", "loop_results.json"))
    args = ap.parse_args()
    tasks = llm.TASKS[:args.tasks]
    ks = [int(x) for x in args.ks.split(",")]
    t0 = time.time()
    results = []
    for k in ks:
        for task in tasks:
            print("== LOOP task=%s k=%d ==" % (task["name"], k), flush=True)
            res = converge(task, k, args.max_cycles)
            results.append(res)
            print("   -> %s converged=%s cycles=%d" % (res["task"], res["converged"], res["cycles"]), flush=True)
    with open(args.json, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    print("\nwall time: %.1fs" % (time.time() - t0))
    for r in results:
        print("  %-16s k=%d converged=%-5s cycles=%d" % (r["task"], r["k"], r["converged"], r["cycles"]))
    # 汇总 compile 失败/playtest 失败总次数
    c_fail = sum(1 for r in results for l in r["log"] if l["stage"] == "compile" and not l["ok"])
    p_fail = sum(1 for r in results for l in r["log"] if l["stage"] == "playtest" and not l["ok"])
    print("compile_failures=%d playtest_failures=%d" % (c_fail, p_fail))


if __name__ == "__main__":
    main()
