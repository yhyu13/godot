#!/usr/bin/env python3
"""010 LLM 收敛度量 — 用 Claude Code headless (claude -p) 测 gdsl 的 LLM 友好度。

冷测：Claude 只看到「gdsl 语法参考 + K 个 few-shot + 任务描述」，禁工具 (--allowedTools '')，
纯生成 (--max-turns 1)。用 `gdslc logic` 判 parse+typecheck 是否有收敛，量 first-try /
iterations-to-valid / valid rate / few-shot 敏感性。

用法 (在仓库根跑)：
    python3 gdsl/toolchain/llm_conv_bench.py               # 默认 4 任务 x K{0,1} x max2 迭代
    python3 gdsl/toolchain/llm_conv_bench.py --tasks 6 --ks 0,1 --max-iter 3
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import time

REPO = r"D:\GitRepo-My\godot"
GDSLC = os.path.join(REPO, "gdsl", "toolchain", "gdslc.exe")
BENCH_DIR = os.path.join(os.environ.get("LOCALAPPDATA", tempfile.gettempdir()), "gdsl_bench")

# claude CLI 在 Windows 上是一个 native claude.exe（npm shim 指向它），
# 而 PATH 里的 "claude" 是 .cmd 垫片 — subprocess(shell=False) 无法用 CreateProcess 解析 .cmd。
# 直接用 native exe，避免 [WinError 2]。
def find_claude():
    p = os.path.join(os.environ.get("APPDATA", r"C:\Users\XINDONG\AppData\Roaming"), "npm",
                     "node_modules", "@anthropic-ai", "claude-code", "bin", "claude.exe")
    if os.path.exists(p):
        return p
    import shutil
    return shutil.which("claude") or "claude"


CLAUDE = find_claude()

GRAMMAR = """gdsl 是一个给 Godot 写游戏玩法的"配方"语言。一个 .gdsl 文件可含 1 到多个 type 声明，每个 type 可带 state 与 rule。

语法：
type <Name> @extends <Godot基类名>
    state:
        <field>: <type> = <default>
    rule <RuleName> by <OwnerType> [target <TargetType>]:
        when <ref>.<field> <cmp> <literal>
        then <effect>[, <effect>...]

约定：
- <type> 是 int | float | bool | string，或文件里另一个 type 的名字（表示对象引用，默认只能是 null）。
- string 字段的默认值用双引号括起，例如：name: string = "hero"。
- <ref> 是 self（规则所有者）或 target（跨参与者，需在 rule 头部写 target <TargetType>）。
- <cmp> 是 > >= < <= == != 。
- <effect> 是 ref.field = lit | ref.field += lit | ref.field -= lit | emit(<信号名>)。
- @extends 必须是真实的 Godot 类（Node2D/Area2D/CharacterBody2D/Resource 等）。"""

FEWSHOT_EXAMPLES = {
    1: """type Player @extends CharacterBody2D
state:
    hp: int = 3

rule TakeDamage by Player:
    when self.hp > 0
    then self.hp -= 1""",
    2: """type Player @extends Node2D
state:
    hp: int = 3

type Bullet @extends Area2D
state:
    damage: int = 1
    owner: Player = null""",
}

TASKS = [
    {
        "name": "basic_int",
        "spec": "一个玩家类型 Player 继承 CharacterBody2D，有整数生命字段 hp 默认 3。规则 TakeDamage：由 Player 触发，当 self 的 hp 大于 0 时，self 的 hp 减 1。",
    },
    {
        "name": "float_accel",
        "spec": "一个玩家类型 Player 继承 CharacterBody2D，有浮点速度字段 speed 默认 300.4，以及整数生命字段 hp 默认 3。规则 Accel：由 Player 触发，当 self 的 hp 大于 0 时，self 的 speed 加 5。",
    },
    {
        "name": "string_name",
        "spec": "一个角色类型 Hero 继承 Node2D，有字符串昵称字段 nickname 默认 'hero'，以及整数生命字段 hp 默认 3。不需要规则。",
    },
    {
        "name": "target_emit",
        "spec": "一个子弹类型 Bullet 继承 Area2D，有整数伤害字段 damage 默认 1。一个玩家类型 Player 继承 CharacterBody2D，有整数生命字段 hp 默认 3。规则 OnHit：由 Bullet 触发，target 为 Player，当 target 的 hp 大于 0 时，target 的 hp 减 1，并发出信号 hit。",
    },
    # ---- 复杂/组合任务（A：扩样本，看组合是否打破 100%）----
    {
        "name": "multi_rule_self",
        "kind": "complex",
        "spec": "一个玩家类型 Player 继承 CharacterBody2D，有整数生命字段 hp 默认 10。规则 Heal：由 Player 触发，当 self 的 hp 小于 10 时，self 的 hp 加 2。规则 TakeDamage：由 Player 触发，当 self 的 hp 大于 5 时，self 的 hp 减 1。",
    },
    {
        "name": "float_emit",
        "kind": "complex",
        "spec": "一个玩家类型 Player 继承 CharacterBody2D，有浮点速度字段 speed 默认 100.0，以及整数生命字段 hp 默认 3。规则 Dash：由 Player 触发，当 self 的 hp 大于 0 时，self 的 speed 加 25，并发出信号 dash。",
    },
    {
        "name": "string_with_rule",
        "kind": "complex",
        "spec": "一个角色类型 Hero 继承 Node2D，有字符串昵称字段 nickname 默认 'hero'，整数生命字段 hp 默认 3。规则 Hurt：由 Hero 触发，当 self 的 hp 大于 0 时，self 的 hp 减 1。",
    },
    {
        "name": "two_types_cross",
        "kind": "complex",
        "spec": "一个子弹类型 Bullet 继承 Area2D，有整数伤害字段 damage 默认 1。规则 Fade：由 Bullet 触发，当 self 的 damage 大于 0 时，self 的 damage 减 1。一个玩家类型 Player 继承 CharacterBody2D，有整数生命字段 hp 默认 5。规则 OnHit：由 Bullet 触发，target 为 Player，当 target 的 hp 大于 0 时，target 的 hp 减 1，并发出信号 hit。",
    },
]


def build_prompt(task, k):
    prompt = GRAMMAR + "\n\n"
    if k > 0:
        prompt += "示例 %d：\n```\n%s\n```\n\n" % (k, FEWSHOT_EXAMPLES[k])
    prompt += "任务描述：%s\n\n" % task["spec"]
    prompt += ("请根据上面的语法和示例，用 gdsl 写出实现该任务描述的一个 .gdsl 文件。"
               "只输出 .gdsl 本身（从 `type` 行开始），不要任何解释、注释、markdown 代码块标记，也不要用任何工具。")
    return prompt


def claude_generate(prompt):
    cmd = [CLAUDE, "-p", prompt, "--allowedTools", "", "--max-turns", "1",
           "--output-format", "json"]
    os.makedirs(BENCH_DIR, exist_ok=True)
    r = subprocess.run(cmd, cwd=BENCH_DIR, capture_output=True, text=True, timeout=180)
    try:
        d = json.loads(r.stdout)
    except json.JSONDecodeError:
        return {"text": r.stdout, "cost": 0.0, "ms": 0, "err": r.stderr[:500]}
    return {"text": d.get("result", ""), "cost": d.get("total_cost_usd", 0.0),
            "ms": d.get("duration_ms", 0), "err": r.stderr[:500]}


def extract_gdsl(text):
    # 剥 markdown 代码块围栏 + 取第一个 type 行到末尾第一个空块之间的正文
    lines = text.splitlines()
    out = []
    in_block = False
    for ln in lines:
        s = ln.strip()
        if s.startswith("```"):
            # 围栏开关：切换 in_block；围栏行本身去掉
            if not in_block and ("type" in s or s == "```" or s.startswith("```gdsl")):
                in_block = True
                continue
            elif in_block:
                in_block = False
                continue
            else:
                # 非围栏开关的行（如 ``` 后面无 gdsl），忽略
                continue
        # 去掉行内 ``` 残留
        out.append(ln.replace("```", ""))
    body = "\n".join(out)
    # 从第一个 "type " 行截起（去掉开头解释性文字）
    idx = body.find("type ")
    if idx >= 0:
        body = body[idx:]
    return body.strip()


def validate(gsdl_text, workdir):
    in_path = os.path.join(workdir, "probe.gdsl")
    out_path = os.path.join(workdir, "probe.c")
    with open(in_path, "w", encoding="utf-8") as f:
        f.write(gsdl_text + ("\n" if gsdl_text else ""))
    r = subprocess.run([GDSLC, "logic", in_path, out_path],
                       capture_output=True, text=True, timeout=60)
    return r.returncode == 0, r.stderr.strip() + r.stdout.strip()


def run_task(task, k, max_iter, stats):
    prompt = build_prompt(task, k)
    results = []
    for it in range(1, max_iter + 1):
        gen = claude_generate(prompt)
        gsdl = extract_gdsl(gen["text"])
        with tempfile.TemporaryDirectory(prefix="gdslbench_") as wd:
            ok, err = validate(gsdl, wd)
        results.append({"iter": it, "ok": ok, "cost": gen["cost"], "ms": gen["ms"],
                        "gsdl": gsdl, "err": err[:200]})
        if ok:
            return {"task": task["name"], "kind": task.get("kind", "easy"), "k": k,
                    "iterations_to_valid": it, "first_try": (it == 1), "valid": True,
                    "cost": gen["cost"], "results": results}
        # 失败：把报错追加进提示，让 LLM 改
        prompt += ("\n\n你上一次输出未通过编译校验，报错如下：\n%s\n"
                   "请修正，并再次只输出修正后的完整 .gdsl。\n" % err)
    # 未收敛
    return {"task": task["name"], "kind": task.get("kind", "easy"), "k": k,
            "iterations_to_valid": max_iter + 1, "first_try": False, "valid": False,
            "cost": sum(r["cost"] for r in results), "results": results}


def run(tasks, ks, max_iter):
    all_results = []
    for k in ks:
        for task in tasks:
            print("== task=%s k=%d ==" % (task["name"], k), flush=True)
            res = run_task(task, k, max_iter, None)
            all_results.append(res)
            print("   -> %s iterations=%d first_try=%s valid=%s" % (
                res["task"], res["iterations_to_valid"], res["first_try"], res["valid"]), flush=True)
    return all_results


def summarize(all_results, ks):
    for k in ks:
        group = [r for r in all_results if r["k"] == k]
        n = len(group)
        first = sum(1 for r in group if r["first_try"])
        valid = sum(1 for r in group if r["valid"])
        iters = [r["iterations_to_valid"] for r in group]
        cost = sum(r["cost"] for r in group)
        mean_it = sum(iters) / len(iters) if iters else 0
        max_it = max(iters) if iters else 0
        print("\n[K=%d] tasks=%d  first_try_rate=%.2f (%d/%d)  valid_rate=%.2f (%d/%d)"
              "  mean_iterations=%.1f  max_iterations=%d  cost=$%.4f"
              % (k, n, first / n if n else 0, first, n, valid / n if n else 0, valid,
                 n, mean_it, max_it, cost))
        # 按 easy/complex 分开报
        for kind in ("easy", "complex"):
            sub = [r for r in group if r.get("kind", "easy") == kind]
            if not sub:
                continue
            n2 = len(sub)
            f2 = sum(1 for r in sub if r["first_try"])
            v2 = sum(1 for r in sub if r["valid"])
            print("    [%s] (of %s) tasks=%d  first_try_rate=%.2f (%d/%d)  valid_rate=%.2f (%d/%d)"
                  % (kind, k, n2, f2 / n2 if n2 else 0, f2, n2, v2 / n2 if n2 else 0, v2, n2))
    print("\n--- per-task detail ---")
    for r in all_results:
        status = "ITER=%d%s%s" % (r["iterations_to_valid"], " FIRST" if r["first_try"] else "",
                                  "" if r["valid"] else " NOTVALID")
        print("  %-16s %-8s k=%d %-26s valid=%s" % (r["task"], r.get("kind", "easy"), r["k"], status, r["valid"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tasks", type=int, default=-1, help="run first N tasks (default all)")
    ap.add_argument("--ks", default="0,1", help="comma-separated few-shot counts")
    ap.add_argument("--max-iter", type=int, default=2)
    ap.add_argument("--json", default=os.path.join(REPO, "gdsl", "bench_results.json"))
    args = ap.parse_args()

    tasks = TASKS
    if args.tasks > 0:
        tasks = tasks[:args.tasks]
    ks = [int(x) for x in args.ks.split(",")]

    t0 = time.time()
    results = run(tasks, ks, args.max_iter)
    with open(args.json, "w", encoding="utf-8") as f:
        json.dump(results, f, ensure_ascii=False, indent=2)
    print("\nwall time: %.1fs" % (time.time() - t0))
    summarize(results, ks)


if __name__ == "__main__":
    main()
