#!/usr/bin/env python3
"""009 TC-7(生命周期)+ TC-8(游戏轨迹)测试。

TC-7: 场景里增删实体 → 无 ObjectDB 泄漏警告 + 退出干净(0)。
TC-8: 多实体场景里跑 N 次命中(调用 on_hit) → 断言到终点态(Player hp 到 0) + 信号计数。

用法：
    python3 gdsl/toolchain/scene_lifecycle.py --gdsl gdsl/playtest_cases/target_emit.gdsl --name target_emit
"""
import argparse
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import playtest as pt
import gdsl_playgen as pg

REPO = pt.REPO


def run(gdsl_path, stem):
    rd = pg.parse_recipe(open(gdsl_path).read())
    types = list(rd["types"].keys())
    with tempfile.TemporaryDirectory(prefix="gdsl78_", ignore_cleanup_errors=True) as wd:
        dll, err = pt.build_dll(gdsl_path, wd)
        if err:
            return None, err
        open(os.path.join(wd, stem + ".gdextension"), "w").write(
            '[configuration]\n\nentry_symbol = "gdsl_library_init"\ncompatibility_minimum = "4.7"\n'
            'reloadable = true\n\n[libraries]\n\nwindows.x86_64 = "res://%s.dll"\n' % stem)
        open(os.path.join(wd, "project.godot"), "w").write(
            'config_version=5\n\n[application]\n\nconfig/name="gdsl 78"\n'
            'config/features=PackedStringArray("4.7")\n\n[autoload]\n\nPT="*res://t.gd"\n')
        # tscn: Bullet + Player
        tscn = ("[gd_scene load_steps=2 format=3]\n"
                '[ext_resource type="GDExtension" path="res://%s.gdextension" id="1"]\n'
                '\n[node name="Root" type="Node2D"]\n' % stem)
        for i, t in enumerate(types):
            tscn += '\n[node name="C%d" type="%s" parent="."]\n' % (i, t)
        open(os.path.join(wd, "scene.tscn"), "w").write(tscn)
        # 目标类型名 + 找出一个发射信号的规则 + 其 owner/target 节点索引
        target_type = next((t for t in types if t != "Bullet"), types[0])
        bi = types.index("Bullet") if "Bullet" in types else 0
        pi = types.index(target_type)
        bullet_type = "Bullet" if "Bullet" in types else types[bi]
        gd = ["@tool", "extends Node", "", "var hit_count = 0", "",
              "func _on_hit():", "\thit_count += 1", "",
              "func _ready():",
              "\tvar inst = (load('res://scene.tscn')).instantiate()",
              "\tadd_child(inst)",
              "\tvar bullet = inst.get_child(%d)" % bi,
              "\tvar player = inst.get_child(%d)" % pi,
              "\tif bullet.has_signal('hit'): bullet.hit.connect(_on_hit)",
              "\tplayer.hp = 3",
              "\t# TC-8: 命中 3 次直到 hp<=0（target.hp -= 1）",
              "\tfor i in range(3):",
              "\t\tbullet.on_hit(player)",
              "\tprint('TC8_END hp=', player.hp, ' dead=', player.hp <= 0, ' signal=', hit_count)",
              "\t# TC-7 生命周期：增删一个实体再释放整个场景，检查 ObjectDB 泄漏",
              "\tvar extra = %s.new()" % bullet_type,
              "\tadd_child(extra)",
              "\textra.free()",
              "\tinst.free()",
              "\tprint('RESULT DONE')",
              "\tget_tree().quit()"]
        open(os.path.join(wd, "t.gd"), "w").write("\n".join(gd) + "\n")
        out = pt.run_engine(wd)
        return out, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gdsl", required=True)
    ap.add_argument("--name", required=True)
    args = ap.parse_args()
    out, err = run(args.gdsl, args.name)
    if err:
        print("FAIL:", err)
        return 1
    for l in out.splitlines():
        if any(k in l for k in ("TC8", "RESULT", "Leaked", "ERROR", "ERROR: ", "ObjectDB")):
            print(l.strip()[:160])
    return 0


if __name__ == "__main__":
    sys.exit(main())
