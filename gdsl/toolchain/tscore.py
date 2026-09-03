#!/usr/bin/env python3
"""gdsl TASTE SCORE — 秒级、无 LLM、隔夜安全的多 agent 竞争度量。

计算一个 [0,100] 的 T 值 + 5 个子分，全部来自仓库内真实产物（不调 LLM、不启引擎）：
  C  convergence    LLM 收敛速度  -> gdsl/bench_results.json (FR-006/007/008)
  V  verifiability  可验证性      -> doctest 套件 + FR-005/009
  E  expressiveness 表达能力      -> playtest_cases/*.gdsl 全管线通过率 (FR-010)
  D  determinism    确定性/免漂移 -> codegen golden 逐字节 (在 doctest 内, 作 law)
  Q  economy        品味/克制     -> 每构造覆盖的真实任务比 (奖励克制、惩罚膨胀)

Law floor (二值门, 违约即 T=0 / DISQUALIFIED):
  L1  doctest 全绿 (含 determinism golden + FR-009 报错定位测试)
  L2  结构覆盖不低于 COVERAGE_FLOOR (避免"简化到坏")

用法 (仓库根):  python3 gdsl/toolchain/tscore.py
                    [--json gdsl/tscore_report.json]
                    [--leader .wolf/tscore_leaderboard.jsonl]
                    [--ontology-syms 4] [--economy-target 0.6] [--coverage-floor 6]
                    [--easy-task-cap N]   # convergence 只统计 easy 任务 (hard 单独列)
"""
import argparse, json, os, re, subprocess, sys, datetime

HERE = os.path.dirname(os.path.abspath(__file__))
GDSL = os.path.normpath(os.path.join(HERE, ".."))
REPO = os.path.normpath(os.path.join(GDSL, ".."))

GDSLC = os.path.join(GDSL, "toolchain", "gdslc.exe")
TEST_EXE = os.path.join(GDSL, "test_gdsl.exe")
BENCH_JSON = os.path.join(GDSL, "bench_results.json")
PLAYTEST_DIR = os.path.join(GDSL, "playtest_cases")
PARSER_CPP = os.path.join(GDSL, "parser.cpp")

W = {"C": 0.30, "V": 0.25, "E": 0.25, "Q": 0.20}   # D determinism 归入 law, 不计权


# ---------------- 底层采集：真实产物 ----------------

def _err_quality():
    """FR-009 错误质量测试是否在测试套件里(测源码 grep TEST_CASE 名, 不依赖运行时输出)."""
    def name_in(fname, needle):
        p = os.path.join(GDSL, fname)
        if not os.path.exists(p):
            return False
        return needle in open(p, encoding="utf-8", errors="ignore").read()
    return {
        "parser_line": name_in("test_parser.cpp", "[GDSL] Parser error carries a line number"),
        "typecheck_symbol": name_in("test_typecheck.cpp", "[GDSL] Typecheck error names the offending symbol"),
    }


def doctest_summary():
    """跑 test_gdsl.exe, 取 [doctest] test cases / assertions 摘要; FR-009 单独从源码 grep."""
    if not os.path.exists(TEST_EXE):
        return None
    r = subprocess.run([TEST_EXE], capture_output=True, text=True, timeout=120)
    txt = r.stdout + r.stderr
    m = re.search(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed", txt)
    a = re.search(r"assertions:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed", txt)
    if not m or not a:
        return {"status": "UNPARSED", "exit": r.returncode, "tail": txt[-400:]}
    passed = int(m.group(2)); total = int(m.group(1))
    passed_a = int(a.group(2)); total_a = int(a.group(1))
    return {"status": "OK", "exit": r.returncode,
            "test_cases": total, "passed": passed, "all_pass": passed == total,
            "assertions": total_a, "assertions_passed": passed_a,
            "err_quality": _err_quality()}


def playtest_coverage():
    """结构覆盖: 每个 playtest_cases/*.gdsl 过 gdslc logic (parse+typecheck+codegen)."""
    if not os.path.exists(GDSLC):
        return None
    cases = sorted(glob_playtest())
    if not cases:
        return {"status": "NO_CASES", "passed": 0, "total": 0}
    passed = 0; fails = []
    tmp_out = os.path.join(GDSL, "toolchain", "_tscore_probe.c")
    for c in cases:
        r = subprocess.run([GDSLC, "logic", c, tmp_out], capture_output=True, text=True, timeout=60)
        if r.returncode == 0:
            passed += 1
        else:
            fails.append(os.path.basename(c))
    return {"status": "OK", "passed": passed, "total": len(cases), "fails": fails}


def glob_playtest():
    import glob
    return glob.glob(os.path.join(PLAYTEST_DIR, "*.gdsl"))


def convergence():
    """LLM 收敛: bench_results.json (FR-006/007/008)."""
    if not os.path.exists(BENCH_JSON):
        return None
    try:
        data = json.load(open(BENCH_JSON, encoding="utf-8"))
    except Exception:
        return None
    if not data:
        return None
    n = len(data)
    first = sum(1 for x in data if x.get("first_try"))
    valid = sum(1 for x in data if x.get("valid"))
    iters = [x.get("iterations_to_valid", 99) for x in data]
    mean_it = sum(iters) / len(iters) if iters else 0
    min_k = min((x.get("k", 99) for x in data), default=99)
    return {"n": n, "first_try_rate": first / n, "valid_rate": valid / n,
            "mean_iters": mean_it, "min_k": min_k}


def economy(grammar_prods, ontology_syms, covered, target):
    constructs = max(1, grammar_prods + ontology_syms)
    ratio = covered / constructs
    sub = min(1.0, ratio / target)       # 越接近/超过 target 越满
    return {"grammar_prods": grammar_prods, "ontology_syms": ontology_syms,
            "constructs": constructs, "covered": covered,
            "ratio": ratio, "target": target, "sub": sub}


def grammar_productions():
    if not os.path.exists(PARSER_CPP):
        return 0
    txt = open(PARSER_CPP, encoding="utf-8", errors="ignore").read()
    names = set(re.findall(r"\b(parse_[a-z_]+)\s*\(", txt))
    return len(names) - (1 if "parse_program" in names else 0)   # parse_program 是入口非产生式


# ---------------- FR-005 mutation harness (validator 真实抓突变率) ----------------
# 每个 mutant 都是「看起来合法但有隐性正确性风险」的输入: 若被 gdslc logic 放行→真缺口(会编译成错/失效代码)。
MUTANTS = {
    "float_to_int":   "type P @extends CharacterBody2D\nstate:\n    hp: int = 3.0",
    "string_num_lit": "type P @extends CharacterBody2D\nstate:\n    name: string = 123",
    "string_from_num":"type P @extends CharacterBody2D\nstate:\n    hp: string = 3",
    "unknown_field":  "type P @extends CharacterBody2D\nstate:\n    hp: int = 3\nrule R by P:\n    when self.nope > 0\n    then self.hp -= 1",
    "target_no_clause":"type P @extends CharacterBody2D\nstate:\n    hp: int = 3\nrule R by P:\n    when target.hp > 0\n    then self.hp -= 1",
    "typo_signal":    "type P @extends CharacterBody2D\nstate:\n    hp: int = 3\nrule R by P:\n    when self.hp > 0\n    then self.hp -= 1, emit(hitt)",
    "unknown_extends":"type P @extends NotAClass\nstate:\n    hp: int = 3",
    "unknown_target_type":"type B @extends Area2D\nstate:\n    d: int = 1\ntype P @extends CharacterBody2D\nstate:\n    hp: int = 5\nrule R by B target Missing:\n    when target.hp > 0\n    then target.hp -= 1",
    "dup_type":       "type P @extends CharacterBody2D\nstate:\n    hp: int = 3\ntype P @extends Node2D\nstate:\n    hp: int = 5",
    "dup_field":      "type P @extends CharacterBody2D\nstate:\n    hp: int = 3\n    hp: int = 5",
    "bad_effect_literal":"type P @extends CharacterBody2D\nstate:\n    hp: int = 3\nrule R by P:\n    when self.hp > 0\n    then self.hp += \"x\"",
    "emit_with_args": "type P @extends CharacterBody2D\nstate:\n    hp: int = 3\nrule R by P:\n    when self.hp > 0\n    then emit(hit, 1)",
    "dup_rule_name":  "type P @extends CharacterBody2D\nstate:\n    hp: int = 3\nrule R by P:\n    when self.hp > 0\n    then self.hp -= 1\nrule R by P:\n    when self.hp < 5\n    then self.hp += 1",
    "int_overflow":   "type P @extends CharacterBody2D\nstate:\n    hp: int = 99999999999999999999",
    "guard_type_mix": "type P @extends CharacterBody2D\nstate:\n    spd: float = 1.5\nrule R by P:\n    when self.spd == 5\n    then self.spd += 1.0",
    "bad_field_ident":"type P @extends CharacterBody2D\nstate:\n    1hp: int = 3",   # 字段名以数字开头 -> 生成 C 的 int64_t 1hp; 非法标识符
}


def mutation_score():
    """FR-005: validator 對 12 个 mutant 的真实拒收率 (gdslc logic, 无 LLM 无引擎)."""
    if not os.path.exists(GDSLC):
        return None
    import tempfile
    caught = 0; gaps = []
    with tempfile.TemporaryDirectory(prefix="gdsl_mut_") as wd:
        tmp = os.path.join(wd, "_mut_input.gdsl")
        out_c = os.path.join(wd, "_mut_probe.c")
        for name, src in MUTANTS.items():
            with open(tmp, "w", encoding="utf-8") as f:
                f.write(src + "\n")
            try:
                r = subprocess.run([GDSLC, "logic", tmp, out_c], capture_output=True, text=True, timeout=60)
                ok = (r.returncode != 0)
            except Exception:
                ok = False
            if ok:
                caught += 1
            else:
                gaps.append(name)
    n = len(MUTANTS)
    return {"caught": caught, "total": n, "score": caught / n if n else 0.0, "gaps": gaps}


# ---------------- 子分归一 ----------------

def sub_convergence(c):
    if not c or c["n"] == 0:
        return None
    iter_ok = max(0.0, 1.0 - (c["mean_iters"] - 1.0) / 4.0)     # 1.0->1.0, 3.0->0.5, >=5->0
    return 0.50 * c["first_try_rate"] + 0.30 * c["valid_rate"] + 0.20 * iter_ok


def sub_verifiability(v, mutation):
    """V = law(all_pass) + err_quality(FR-009) + mutation_score(FR-005). NA 维归一."""
    if not v or v.get("status") != "OK":
        return None
    comps = {}
    comps["law"] = (0.50, 1.0 if v["all_pass"] else 0.0)
    err = v["err_quality"]
    err_q = sum(1 for x in err.values() if x) / max(1, len(err))
    comps["err_q"] = (0.20, err_q)
    if mutation:
        comps["mutation"] = (0.30, mutation["score"])
    wsum = sum(w for w, _ in comps.values())
    return sum(w * val for w, val in comps.values()) / wsum


def sub_expressiveness(e):
    if not e or e.get("status") != "OK" or e["total"] == 0:
        return None
    return e["passed"] / e["total"]


def semantic_e():
    """E(语义): semantic_results.json 里 ALLPASS/(ALLPASS+FAIL)。ERR 视为无法验证 = NA, 不算过也不故意拉低.
    文件不存在 -> None (回退结构口径)。"""
    p = os.path.join(GDSL, "semantic_results.json")
    if not os.path.exists(p):
        return None
    try:
        data = json.load(open(p, encoding="utf-8"))
    except Exception:
        return None
    cases = [v for v in data.values() if v.get("status") in ("ALLPASS", "FAIL")]
    if not cases:
        return None
    ok = sum(1 for v in cases if v.get("status") == "ALLPASS")
    return {"passed": ok, "total": len(cases), "status": "OK"}


# ---------------- 主入口 ----------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=os.path.join(GDSL, "tscore_report.json"))
    ap.add_argument("--leader", default=os.path.join(REPO, ".wolf", "tscore_leaderboard.jsonl"))
    ap.add_argument("--ontology-syms", type=int, default=4, help="effect ontology v1 = assign/add/sub/emit (enum, 非字符串不可 grep)")
    ap.add_argument("--economy-target", type=float, default=0.90, help="品味门槛: 每构造覆盖的真实任务数(越贴近1越克制; 当前0.80是真实非最优)")
    ap.add_argument("--coverage-floor", type=int, default=6, help="law L2: 结构覆盖下限")
    ap.add_argument("--valid-rate-floor", type=float, default=0.75, help="law L3: convergence valid_rate 下限(LLM 能否写出)")
    ap.add_argument("--fingerprint", action="store_true",
                    help="只算分打印一行确定性指纹(不 append leaderboard), 供 cron monitor 自检空转")
    args = ap.parse_args()

    dt = doctest_summary()
    cov = playtest_coverage()
    conv = convergence()
    prods = grammar_productions()
    eco = economy(prods, args.ontology_syms, (cov or {}).get("passed", 0), args.economy_target)
    mut = mutation_score()
    se = semantic_e()

    S = {
        "C": sub_convergence(conv),
        "V": sub_verifiability(dt, mut),
        "E": sub_expressiveness(se) if se else sub_expressiveness(cov),
        "Q": eco["sub"],
    }

    # ---- law floor ----
    law = []
    if dt and dt.get("status") == "OK":
        law.append(("L1-doctest-all-green", dt["all_pass"]))
    else:
        law.append(("L1-doctest-all-green", False))
    floor_ok = (cov or {}).get("passed", 0) >= args.coverage_floor
    law.append(("L2-coverage-floor>=%d" % args.coverage_floor, floor_ok))
    # L3: LLM 能否写出合法 GDSL (convergence valid_rate 门槛) —— 有 bench 数据才判, 无则视为通过
    vr_floor_ok = (conv is None) or (conv["valid_rate"] >= args.valid_rate_floor)
    law.append(("L3-valid-rate>=%.2f" % args.valid_rate_floor, vr_floor_ok))
    laws_pass = all(x[1] for x in law)

    # ---- composite: 仅现维加权, NA 维归一 ----
    present = {k: v for k, v in S.items() if v is not None}
    if not present:
        T = 0.0
    else:
        wsum = sum(W[k] for k in present)
        T = sum(W[k] * present[k] for k in present) / wsum
    score = round(T * 100, 1)
    if not laws_pass:
        score = 0.0          # 违约即不合格

    report = {
        "ts": datetime.datetime.now().isoformat(timespec="seconds"),
        "score": score, "laws_pass": laws_pass, "laws": [list(x) for x in law],
        "weights_used": {k: W[k] for k in present},
        "subs": {k: (round(v, 4) if v is not None else None) for k, v in S.items()},
        "raw": {
            "doctest": dt, "coverage": cov, "convergence": conv,
            "economy": eco, "grammar_prods": prods, "mutation": mut,
            "E_mode": "semantic" if se else "structural",
            "semantic_e": se,
        },
        "note": naive_note(S, conv),
    }

    os.makedirs(os.path.dirname(args.json), exist_ok=True)
    if args.fingerprint:
        # 只算分, 打一行确定性指纹(无时间戳), 供 cron monitor 判断空转. 不 append leaderboard.
        g = (mut or {}).get("gaps") or []
        gaps = ",".join(sorted(g)) if g else "none"
        v = present.get("V", 0.0)
        mut_s = "%s/%s" % (mut["caught"], mut["total"]) if mut else "na"
        print("score=%s|V=%s|mut=%s|gaps=%s" % (score, round(v, 3), mut_s, gaps))
        return score
    with open(args.json, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    append_leader(args.leader, report, score)

    print_human(report, args)
    return score


def naive_note(S, conv):
    notes = []
    if S["C"] is not None and S["C"] >= 0.999:
        notes.append("convergence at ceiling (>=0.999): easy set saturated, no competitive headroom")
    if S["Q"] is not None and S["Q"] < 0.99:
        notes.append("economy < 1.0: real taste headroom here (capability per construct)")
    if conv is None:
        notes.append("bench_results.json absent -> convergence excluded, weights renormalized")
    return "; ".join(notes)


def append_leader(path, report, score):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    line = {"ts": report["ts"], "score": score, "subs": report["subs"], "laws_pass": report["laws_pass"]}
    with open(path, "a", encoding="utf-8") as f:
        f.write(json.dumps(line, ensure_ascii=False) + "\n")


def print_human(r, args):
    print("=== gdsl TASTE SCORE ===")
    print("T = %.1f/100   laws_pass=%s" % (r["score"], r["laws_pass"]))
    for k, v in r["subs"].items():
        print("  %-2s %s" % (k, ("%.4f" % v) if v is not None else "NA(excluded)"))
    print("weights used:", r["weights_used"])
    print("laws:", r["laws"])
    if r["raw"]["convergence"]:
        c = r["raw"]["convergence"]
        print("conv: n=%d first_try=%.2f valid=%.2f mean_iters=%.2f min_k=%s" % (
            c["n"], c["first_try_rate"], c["valid_rate"], c["mean_iters"], c["min_k"]))
    if r["raw"]["coverage"] and r["raw"]["coverage"].get("status") == "OK":
        cc = r["raw"]["coverage"]
        print("playtest coverage: %d/%d%s" % (cc["passed"], cc["total"], ((" fails=" + ",".join(cc["fails"])) if cc["fails"] else "")))
    ec = r["raw"]["economy"]
    print("economy: prods=%d ontology_syms=%d constructs=%d covered=%d ratio=%.3f target=%.2f -> sub=%.3f" % (
        ec["grammar_prods"], ec["ontology_syms"], ec["constructs"], ec["covered"], ec["ratio"], ec["target"], ec["sub"]))
    m = r["raw"].get("mutation")
    if m:
        print("mutation FR-005: %d/%d=%0.3f  gaps=%s" % (m["caught"], m["total"], m["score"], ",".join(m["gaps"]) or "none"))
    print("DOCTEST:", (r["raw"]["doctest"].get("status") if r["raw"]["doctest"] else "exe missing"))
    if r["note"]:
        print("note:", r["note"])
    print("report ->", args.json)
    print("leader ->", args.leader)


if __name__ == "__main__":
    main()
