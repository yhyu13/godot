#!/usr/bin/env python3
"""cron monitor 自检脚本: 判断 taste score 有没有「活可干」, 有则输出变化的指纹, 无则输出固定常量.

Hermes cron 的 `monitor` 字段每 tick 跑本脚本, 比较 stdout 与上次: 相同 -> 跳过 agent(空转零成本);
不同 -> 唤醒 agent. 用 tscore.py --fingerprint 的确定性指纹(无时间戳):
  - 分数饱和(score=100 且 gaps=none) -> 输出恒定, agent 不再空转.
  - 有新 gap / 分数有空间 / 测量被再武装(扩 mutant 集) -> 指纹变化 -> agent 醒来干活.

用法: 在 godot cron 上设 monitor = 本脚本的绝对路径.
"""
import os, subprocess, sys

REPO = r"D:\GitRepo-My\godot"
TSCORE = os.path.join(REPO, "gdsl", "toolchain", "tscore.py")


def main():
    r = subprocess.run([sys.executable, TSCORE, "--fingerprint"],
                       capture_output=True, text=True, cwd=REPO, timeout=120)
    out = r.stdout.strip()
    if r.returncode != 0:
        # 异常: 输出一个"有活/需检查"信号避免可被当作无活而一直跳过; 但无时间戳以保持确定性.
        print("ERR score=na|V=na|mut=na|gaps=unknown")
        return 1
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
