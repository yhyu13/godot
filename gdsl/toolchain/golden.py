#!/usr/bin/env python3
"""方向 2 —— 语义对 spec 金标准校验（抓「LLM 写反了」）。

对比现有 playtest 的关键区别：
- playtest.py 生成器 = 读配方的 effect，断言「配方自己写的 effect 执行了」（循环论证，写反也能过）。
- golden.py = 读人类定义的金标准（期望行为），断言「配方运行时行为 == 金标准」。
  这样 `then self.hp += 1`（本应 -=1）会被金标准 hp==4 抓出来。

金标准 = 每个任务一组 scenario：
  { "rule": <规则名>, "setup_owner": {字段:值}, "setup_target": {字段:值},
    "expect_owner": {字段:值}, "expect_target": {字段:值} }
即：设初始状态 → 调规则方法 → 断言某些字段到了期望值。

用法：
  python3 gdsl/toolchain/golden.py        # 跑 3 个任务的自测（正确配方 PASS + 写反配方 FAIL）
  # 之后可把 gen_golden_playtest_gd 接进 loop_bench 的收敛条件。
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gdsl_playgen as pg
from gdsl_playgen import _snake


# 金标准：由我按 task spec 定义「正确行为」（用户可审可改）。
# 期望值 = spec 想要的结果，不是配方自己写的 effect。
# rule="__default__" 表示该任务无规则，只断言实例化默认状态。
GOLDEN = {
    "basic_int": [
        {"rule": "TakeDamage", "setup_owner": {"hp": 5}, "setup_target": {},
         "expect_owner": {"hp": 4}, "expect_target": {}},
        {"rule": "TakeDamage", "setup_owner": {"hp": 0}, "setup_target": {},
         "expect_owner": {"hp": 0}, "expect_target": {}},   # guard hp>0 挡掉
    ],
    "float_accel": [
        {"rule": "Accel", "setup_owner": {"speed": 300.4}, "setup_target": {},
         "expect_owner": {"speed": 305.4}, "expect_target": {}},
    ],
    "string_name": [
        {"rule": "__default__", "setup_owner": {}, "setup_target": {},
         "expect_owner": {"nickname": "hero", "hp": 3}, "expect_target": {}},
    ],
    "target_emit": [
        {"rule": "OnHit", "setup_owner": {"damage": 2}, "setup_target": {"hp": 10},
         "expect_owner": {}, "expect_target": {"hp": 9}},
    ],
    "multi_rule_self": [
        {"rule": "Heal", "setup_owner": {"hp": 5}, "setup_target": {},
         "expect_owner": {"hp": 7}, "expect_target": {}},
        {"rule": "TakeDamage", "setup_owner": {"hp": 6}, "setup_target": {},
         "expect_owner": {"hp": 5}, "expect_target": {}},
    ],
    "float_emit": [
        {"rule": "Dash", "setup_owner": {"speed": 100.0}, "setup_target": {},
         "expect_owner": {"speed": 125.0}, "expect_target": {}},   # emit 未断言
    ],
    "string_with_rule": [
        {"rule": "Hurt", "setup_owner": {"hp": 3}, "setup_target": {},
         "expect_owner": {"hp": 2}, "expect_target": {}},
    ],
    "two_types_cross": [
        {"rule": "Fade", "setup_owner": {"damage": 2}, "setup_target": {},
         "expect_owner": {"damage": 1}, "expect_target": {}},
        {"rule": "OnHit", "setup_owner": {"damage": 1}, "setup_target": {"hp": 5},
         "expect_owner": {}, "expect_target": {"hp": 4}},
    ],
}


def _set_fld(body, var, fld, val):
    body.append("\t%s.%s = %s" % (var, fld, _gv(val)))


def _gv(v):
    if isinstance(v, str):
        return '"%s"' % v          # 字符串值必须加引号，否则 GDScript 当标识符 → 解析错
    if isinstance(v, float) and v.is_integer():
        return str(int(v))
    return str(v)


def gen_golden_playtest_gd(recipe_text, task_name):
    """按 base 任务的金标准生成 GDScript。owner/target 类型从配方解析。"""
    rd = pg.parse_recipe(recipe_text)
    scenarios = GOLDEN.get(task_name, [])
    body = ["@tool", "extends Node", "", "func _ready():", "\tvar all_pass = true",
            "\tvar scenario_ok = true", "\tvar owner = null", "\tvar target = null"]
    # 布局：每个规则的 owner 类型（by）与 target 类型
    rules = {r["name"]: r for r in rd["rules"]}
    for sc in scenarios:
        if sc["rule"] == "__default__":
            # 无规则任务：实例化第一个类型，只断言默认状态
            owner_type = list(rd["types"].keys())[0]
            body.append("\towner = %s.new()" % owner_type)
            body.append("\tscenario_ok = true")
            for fld, val in sc.get("expect_owner", {}).items():
                body.append('\tif (owner.%s != %s): print("FAIL", "%s.%s", "expected", %s, "got", str(owner.%s)); scenario_ok = false'
                            % (fld, _gv(val), owner_type, fld, _gv(val), fld))
            body.append('\tprint("PASS %s/default" if scenario_ok else "FAIL %s/default")' % (task_name, task_name))
            body.append("\tall_pass = all_pass and scenario_ok")
            continue
        rule = rules.get(sc["rule"])
        if not rule:
            body.append('\tprint("FAIL unknown rule %s")' % sc["rule"])
            body.append("\tall_pass = false")
            continue
        owner_type = rule["by"]
        target_type = rule.get("target") or ""
        body.append("\towner = %s.new()" % owner_type)
        has_t = bool(target_type)
        if has_t:
            body.append("\ttarget = %s.new()" % target_type)
        for fld, val in sc.get("setup_owner", {}).items():
            _set_fld(body, "owner", fld, val)
        for fld, val in sc.get("setup_target", {}).items():
            _set_fld(body, "target", fld, val)
        # 调用规则方法
        method = _snake(rule["name"])
        call = "owner.%s(%s)" % (method, "target" if has_t else "")
        body.append("\t%s" % call)
        # 断言 expect（逐字段，FAIL 时记 scenario_ok=false）
        body.append("\tscenario_ok = true")
        for fld, val in sc.get("expect_owner", {}).items():
            body.append('\tif (owner.%s != %s): print("FAIL", "%s.%s", "expected", %s, "got", str(owner.%s)); scenario_ok = false'
                        % (fld, _gv(val), owner_type, fld, _gv(val), fld))
        for fld, val in sc.get("expect_target", {}).items():
            body.append('\tif (target.%s != %s): print("FAIL", "%s.%s", "expected", %s, "got", str(target.%s)); scenario_ok = false'
                        % (fld, _gv(val), target_type, fld, _gv(val), fld))
        body.append('\tprint("PASS %s/%s" if scenario_ok else "FAIL %s/%s")' % (task_name, sc["rule"], task_name, sc["rule"]))
        body.append("\tall_pass = all_pass and scenario_ok")
    body.append('\tprint("RESULT ALLPASS" if all_pass else "RESULT SOME_FAIL")')
    body.append("\tget_tree().quit()")
    return "\n".join(body) + "\n"


# 自测：正确配方 should PASS，写反配方（+=1）should FAIL。
KNOWINGLY_WRONG = {
    "basic_int": """type Player @extends CharacterBody2D
state:
    hp: int = 3
rule TakeDamage by Player:
    when self.hp > 0
    then self.hp += 1
""",
}

if __name__ == "__main__":
    import tempfile, subprocess, os
    import playtest as pt

    def run_gd(recipe_text, task):
        stem = "golden_" + task
        # ignore_cleanup_errors：Windows 上引擎退出后可能短暂锁住 ~*.dll，清不掉不 abort
        with tempfile.TemporaryDirectory(prefix="gdslgold_", ignore_cleanup_errors=True) as wd:
            gdsl_path = os.path.join(wd, stem + ".gdsl")
            with open(gdsl_path, "w", encoding="utf-8") as f:
                f.write(recipe_text)
            dll, err = pt.build_dll(gdsl_path, wd)
            if err:
                return "BUILD FAIL: " + err[:200]
            with open(os.path.join(wd, stem + ".gdextension"), "w") as f:
                f.write("[configuration]\n\nentry_symbol = \"gdsl_library_init\"\ncompatibility_minimum = \"4.7\"\n"
                        "reloadable = true\n\n[libraries]\n\nwindows.x86_64 = \"res://%s.dll\"\n" % stem)
            with open(os.path.join(wd, "project.godot"), "w") as f:
                f.write('config_version=5\n\n[application]\n\nconfig/name="gdsl gold"\n'
                        'config/features=PackedStringArray("4.7")\n\n[autoload]\n\nPT="*res://playtest.gd"\n')
            with open(os.path.join(wd, "playtest.gd"), "w") as f:
                f.write(gen_golden_playtest_gd(recipe_text, task))
            out = pt.run_engine(wd)
            return out

    # 1) 全部 8 个正确配方（从 playtest_cases 读，即 LLM 产出的正确版）→ 应 ALLPASS
    cases = {}
    for task in list(GOLDEN.keys()):
        p = os.path.join("gdsl", "playtest_cases", task + ".gdsl")
        if os.path.exists(p):
            cases[task] = open(p).read()
    for task, recipe in cases.items():
        out = run_gd(recipe, task)
        result = next((l.strip() for l in out.splitlines() if "RESULT" in l), "NO_RESULT")
        print("== %s (correct recipe) == %s" % (task, result))
    # 2) 写反配方（+=1 而非 -=1，编译有效但语义错）→ 应 SOME_FAIL（证明能抓写反）
    wrong = KNOWINGLY_WRONG["basic_int"]
    out = run_gd(wrong, "basic_int")
    print("\n== basic_int (WRONG: +=1 instead of -=1) ==")
    for l in out.splitlines():
        if "PASS" in l or "FAIL" in l or "RESULT" in l:
            print("  ", l.strip()[:120])
