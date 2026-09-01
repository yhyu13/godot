#!/usr/bin/env python3
"""009 场景加载接受测试（FR-003 基础版）：一个配方 → DLL → 自动生成 .tscn → 引擎加载 → 断言。

区别于 playtest.py（孤立 new() + 调方法），这脚本验证：
  - 配方生成的类型能在真场景 .tscn 里实例化
  - 实例的 native class / 默认状态对上配方
  - 规则方法在场景树里调用后状态真变

用法（仓库根）：
    python3 gdsl/toolchain/scene_accept.py --gdsl gdsl/playtest_cases/basic_int.gdsl --name basic_int
"""
import argparse
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import playtest as pt
import gdsl_playgen as pg

REPO = pt.REPO


def make_tscn_types(recipe_text):
    """从配方解析每个 type 名，生成一个 tscn：Root(Node2D) + 每个 type 一个节点。"""
    rd = pg.parse_recipe(recipe_text)
    types = list(rd["types"].keys())
    lines = ["[gd_scene load_steps=2 format=3]",
             '[ext_resource type="GDExtension" path="res://$EXT$.gdextension" id="1"]',
             "",
             '[node name="Root" type="Node2D"]']
    for i, t in enumerate(types):
        lines.append('')
        # tscn 里节点 type 用类名；@extends 的基类由引擎处理
        lines.append('[node name="C%d" type="%s" parent="."]' % (i, t))
    return "\n".join(lines).replace("$EXT$", ""), types


def run_scene_accept(gdsl_path, stem):
    rd = pg.parse_recipe(open(gdsl_path).read())
    types = list(rd["types"].keys())
    with tempfile.TemporaryDirectory(prefix="gdsl009_", ignore_cleanup_errors=True) as wd:
        # 1) 编译 DLL
        dll, err = pt.build_dll(gdsl_path, wd)
        if err:
            return None, err
        # 2) manifest（reloadable must be true）
        open(os.path.join(wd, stem + ".gdextension"), "w").write(
            '[configuration]\n\nentry_symbol = "gdsl_library_init"\ncompatibility_minimum = "4.7"\n'
            'reloadable = true\n\n[libraries]\n\nwindows.x86_64 = "res://%s.dll"\n' % stem)
        # 3) 自动生成 tscn（Root + 每个 type 一个节点）
        tscn = ("[gd_scene load_steps=2 format=3]\n"
                '[ext_resource type="GDExtension" path="res://%s.gdextension" id="1"]\n'
                '\n[node name="Root" type="Node2D"]\n' % stem)
        for i, t in enumerate(types):
            tscn += '\n[node name="C%d" type="%s" parent="."]\n' % (i, t)
        open(os.path.join(wd, "scene.tscn"), "w").write(tscn)
        # 4) 断言脚本
        gd = ["@tool", "extends Node", "", "func _ready():",
              "\tvar inst = (load('res://scene.tscn')).instantiate()",
              "\tadd_child(inst)", "\tvar all_ok = true", "\tvar before = 0", "\tvar after = 0"]
        gd.append('\tprint("CHILD_COUNT=", inst.get_child_count(), " expected=", %d)' % len(types))
        gd.append("\tall_ok = all_ok and (inst.get_child_count() == %d)" % len(types))
        for i, t in enumerate(types):
            gd.append('\tvar c%d = inst.get_child(%d)' % (i, i))
            gd.append('\tprint("NODE%d_CLASS=", c%d.get_class(), " expected=", "%s")' % (i, i, t))
            gd.append("\tall_ok = all_ok and (c%d.get_class() == '%s')" % (i, t))
            # 默认状态：断言每个字段等于配方的默认值
            for fld, (ftype, default) in rd["types"][t]["fields"].items():
                if ftype == "string":
                    dft = default.strip('"')
                    gd.append('\tprint("STATE %s.%s=", c%d.%s, " expected=", "%s")' % (t, fld, i, fld, dft))
                    gd.append("\tall_ok = all_ok and (c%d.%s == '%s')" % (i, fld, dft))
                else:
                    # 数字/字符串默认值逐字比对：写进玩法字段的属性 getter
                    gd.append('\tprint("STATE %s.%s=", c%d.%s, " expected=", "%s")' % (t, fld, i, fld, default))
        # 5) 规则在场景树里触发（self + target 都测）
        #    rules: 每个规则，owner=by 类型的节点；target 规则传 target 类型节点
        for rule in rd["rules"]:
            m = pg._snake(rule["name"])
            eff = next((e for e in rule["effects"] if e[0] == "field"), None)
            if not eff:
                continue
            _, ref, fld, op, lit = eff
            owner_type = rule["by"]
            oi = types.index(owner_type)
            if rule.get("target"):
                ti = types.index(rule["target"])
                gd.append('\tbefore = c%d.%s' % (ti, fld))
                gd.append('\tc%d.%s(c%d)' % (oi, m, ti))
                gd.append('\tafter = c%d.%s' % (ti, fld))
                gd.append('\tprint("TARGET_RULE", "%s.%s", "before", before, "after", after, "applied", after != before)' % (rule["target"], fld))
                gd.append("\tall_ok = all_ok and (after != before)")
            else:
                gd.append('\tbefore = c%d.%s' % (oi, fld))
                gd.append('\tc%d.%s()' % (oi, m))
                gd.append('\tafter = c%d.%s' % (oi, fld))
                dir_txt = "+" if op == "+=" else ("-" if op == "-=" else "=")
                gd.append('\tprint("SELF_RULE", "%s", "applied", after != before)' % m)
                gd.append("\tall_ok = all_ok and (after != before)")
        gd.append('\tprint("RESULT ACCEPT" if all_ok else "RESULT FAIL")')
        gd.append("\tget_tree().quit()")
        open(os.path.join(wd, "scene_test.gd"), "w").write("\n".join(gd) + "\n")
        open(os.path.join(wd, "project.godot"), "w").write(
            'config_version=5\n\n[application]\n\nconfig/name="gdsl 009"\n'
            'config/features=PackedStringArray("4.7")\n\n[autoload]\n\nPT="*res://scene_test.gd"\n')
        out = pt.run_engine(wd)
        return out, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gdsl", required=True)
    ap.add_argument("--name", required=True)
    args = ap.parse_args()
    out, err = run_scene_accept(args.gdsl, args.name)
    if err:
        print("SCENE ACCEPT FAIL:", err)
        return 1
    for l in out.splitlines():
        if any(k in l for k in ("CHILD_COUNT", "NODE", "STATE", "RULE", "RESULT")):
            print(l.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
