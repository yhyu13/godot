#!/usr/bin/env python3
"""gdsl .gdsl 迷你解析器 + 自动 playtest 脚本生成器（FR-011 闭环用）。

给一个 .gdsl 配方，解析它的 type / rule / guard / effect，生成一段 GDScript：
创建实例 → 满足 guard → 调用规则方法 → 断言 effect 字段按规则变化 → 打印 PASS/FAIL。
这样可以把「编译有效」的配方自动跑到真引擎里验证「规则是否真的执行」，
失败时把结果回喂给 LLM 形成闭环（ScriptDoctor 式）。

只覆盖 v1 需要的子集：单比较 guard、ref.field op lit / emit(sig) effect。够本仓库配方用。
"""
import re

CMP = (">=", "<=", "==", "!=", ">", "<")
OPS = ("+=", "-=", "=")


def _snake(name):
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
    return s


def parse_when(body):
    body = body.strip()
    for c in CMP:
        idx = body.find(c)
        if idx >= 0:
            lhs = body[:idx].strip()
            rhs = body[idx + len(c):].strip()
            ref, _, field = lhs.partition(".")
            return ref.strip(), field.strip(), c, rhs
    return None


def parse_effects(body):
    effs = []
    for part in body.split(","):
        part = part.strip()
        if not part:
            continue
        m = re.match(r"emit\((\w+)\)", part)
        if m:
            effs.append(("emit", m.group(1)))
            continue
        for op in OPS:
            if op in part:
                lhs, rhs = part.split(op, 1)
                ref, _, field = lhs.strip().partition(".")
                effs.append(("field", ref.strip(), field.strip(), op, rhs.strip()))
                break
    return effs


def parse_recipe(text):
    """返回 {'types': {name: {base,fields:{name:(type,default)}}}, 'rules': [...]}"""
    types = {}
    rules = []
    cur_rule = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("type "):
            rest = line[5:].strip()
            name, _, base = rest.partition("@extends")
            types[name.strip()] = {"base": base.strip(), "fields": {}}
            cur_rule = None
        elif line == "state:":
            pass
        elif line.startswith("rule "):
            rest = line[5:].rstrip(":").strip()
            name, _, byrest = rest.partition(" by ")
            name = name.strip()
            by, _, targetrest = byrest.partition(" target ")
            target = targetrest.split(":")[0].strip() if targetrest.strip() else ""
            cur_rule = {"name": name, "by": by.strip(), "target": target,
                        "guard": None, "effects": []}
            rules.append(cur_rule)
        elif line.startswith("when "):
            if cur_rule:
                cur_rule["guard"] = parse_when(line[5:].strip())
        elif line.startswith("then "):
            if cur_rule:
                cur_rule["effects"] = parse_effects(line[5:].strip())
        else:
            # 字段行：name: type = default
            m = re.match(r"(\w+):\s*(\w+)\s*=\s*(.+)$", line)
            if m and types:
                types[list(types.keys())[-1]]["fields"][m.group(1)] = (m.group(2), m.group(3))
    return {"types": types, "rules": rules}


def _guard_satisfy(cmp, lit):
    try:
        v = float(lit)
    except ValueError:
        return lit
    if cmp == ">":
        return v + 1 if v.is_integer() else v + 1.0
    if cmp == ">=":
        return v
    if cmp == "<":
        return v - 1 if (v - 1) >= 0 or cmp == "<" else v
    if cmp == "<=":
        return v
    if cmp == "==":
        return v
    if cmp == "!=":
        return v + 1
    return v


def gen_playtest_gd(recipe_text):
    rd = parse_recipe(recipe_text)
    rules = rd["rules"]
    body = ["@tool", "extends Node", "", "func _ready():", "\tvar all_pass = true"]
    for r in rules:
        owner_type = r["by"]
        target_type = r["target"]
        method = _snake(r["name"])
        body.append("\tvar owner = %s.new()" % owner_type)
        if target_type:
            body.append("\tvar target = %s.new()" % target_type)
        # 满足 guard
        guard = r["guard"]
        if guard and guard[0]:
            ref, field, cmp_, lit = guard
            val = _guard_satisfy(cmp_, lit)
            owner_var = "target" if ref == "target" else "owner"
            body.append("\t%s.%s = %s" % (owner_var, field, val))
        # 找第一个 field effect 做断言
        eff = next((e for e in r["effects"] if e[0] == "field"), None)
        call = "owner.%s(%s)" % (method, "target" if target_type else "")
        if eff:
            _, ref, field, op, lit = eff
            owner_var = "target" if ref == "target" else "owner"
            body.append("\tvar before = %s.%s" % (owner_var, field))
            body.append("\t%s" % call)
            body.append("\tvar after = %s.%s" % (owner_var, field))
            if op == "+=":
                chk = "(after - before) == %s" % lit
                desc = "%s %s +%s" % (owner_var, field, lit)
            elif op == "-=":
                chk = "(before - after) == %s" % lit
                desc = "%s %s -%s" % (owner_var, field, lit)
            else:  # =
                chk = "after == %s" % lit
                desc = "%s %s = %s" % (owner_var, field, lit)
            body.append("\tvar ok = %s" % chk)
            body.append('\tprint("PASS %s" if ok else "FAIL %s expect %s got %%s" %% str(after))' % (desc, desc, desc))
            body.append("\tall_pass = all_pass and ok")
        else:
            # 只有 emit 或空 effect——暂不做运行时断言（emit 观察需 signal 声明），调用一次即可
            body.append("\t%s" % call)
    body.append('\tprint("RESULT ALLPASS" if all_pass else "RESULT SOME_FAIL")')
    body.append("\tget_tree().quit()")
    return "\n".join(body) + "\n"


if __name__ == "__main__":
    import sys
    txt = open(sys.argv[1]).read()
    print(gen_playtest_gd(txt))
