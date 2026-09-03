#!/usr/bin/env python3
"""FR-011 语义 E 测量：跑真机引擎 playtest，看 playtest_cases 是否真的「玩对」。

结果写 gdsl/semantic_results.json（每 case: ALLPASS / FAIL / ERR）。E(semantic) = ALLPASS / total。
区别于结构 E（parse+typecheck+codegen 能过），这是「运行时正确」——可能 parse 过但 play 不对（语义 bug）。

用法（仓库根）：python3 gdsl/toolchain/semantic_bench.py
注意：每 case 要 cl 编 .dll + 起引擎，分钟级；跑完才有 E_semantic 基线。
"""
import os, sys, json, glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import loop_bench as lb
import llm_conv_bench as llm

REPO = llm.REPO
GDSL = os.path.join(REPO, "gdsl")
CASES = os.path.join(GDSL, "playtest_cases")
OUT = os.path.join(GDSL, "semantic_results.json")


def main():
    results = {}
    for gdsl in sorted(glob.glob(os.path.join(CASES, "*.gdsl"))):
        stem = os.path.splitext(os.path.basename(gdsl))[0]
        txt = open(gdsl, encoding="utf-8").read()
        try:
            out, err = lb.run_playtest(txt, stem, task_name=stem)
        except Exception as e:  # engine/build 炸了也算 ERR，不算 ALLPASS
            results[stem] = {"status": "ERR", "err": str(e)[:200]}
            print("%-20s ERR %s" % (stem, str(e)[:80]), flush=True)
            continue
        if out is None:
            results[stem] = {"status": "ERR", "err": (err or "")[:200]}
            print("%-20s ERR %s" % (stem, (err or "")[:80]), flush=True)
            continue
        allpass = ("RESULT ALLPASS" in out) or ("ALLPASS" in out)
        fail_lines = [l.strip() for l in out.splitlines() if "FAIL" in l.upper()][:6]
        results[stem] = {"status": "ALLPASS" if allpass else "FAIL",
                         "fails": fail_lines, "err": err or ""}
        print("%-20s %s %s" % (stem, results[stem]["status"], ";".join(fail_lines)[:80]), flush=True)

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    ok = sum(1 for v in results.values() if v["status"] == "ALLPASS")
    n = len(results)
    print("\nSEMANTIC E = %d/%d = %.3f  (ALLPASS=%d FAIL=%d ERR=%d)" % (
        ok, n, (ok / n if n else 0.0),
        ok, sum(1 for v in results.values() if v["status"] == "FAIL"),
        sum(1 for v in results.values() if v["status"] == "ERR")))
    print("fails:", {k: v["fails"] for k, v in results.items() if v["status"] == "FAIL"})


if __name__ == "__main__":
    main()
