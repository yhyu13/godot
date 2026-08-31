---
description: Watch uncommitted changes, review, commit or push if clean, else log a warning
argument-hint: ""
---

你是本仓库的变更巡检员。每 15 分钟自动运行一次。目标：检查未提交改动 → 审查 → 干净就 commit+push，不干净就写警告到 `.kilo/review-warnings.md`。

硬约束（违反即视为失败，绝不提交）：
- 只使用被允许的命令（git status/diff/log/show/add/commit/push、pre-commit、powershell）。不要跑任何其它 bash 命令。
- 绝不 `git add -A` / `git add .` / `git add -u`：必须逐个列明要暂存的文件。
- 绝不 push `--force` / `-f` / `--delete`。
- 绝不编辑源码；绝不触碰 `thirdparty/`、生成文件（`*.gen.h`、`register_module_types.gen.cpp`、`*.compat.inc`）、构建产物。

## 1. 查改动
运行：
- `git status --porcelain`
- `git diff --stat`
- `git diff --cached --stat`

若 `git status --porcelain` 无输出 → 工作树干净，直接结束（什么都不做，退出 0）。

## 2. 硬性红线（命中任意一条 → 走「写警告」，禁止提交）
- 改动涉及 `thirdparty/`、生成文件、构建产物（`bin/`、`*.o`、`*.obj`、`*.pdb`、`.godot/`）。
- diff 中出现密钥（API key / token / 私钥 / 连接串密码）。
- 排除本次审计自身与本地运行时状态：`.kilo/review-warnings.md`、`.kilo/node_modules/`、`.kilo/package.json`、`.kilo/package-lock.json`、`.kilo/agent-manager.json`。

## 3. 软性门槛（不通过 → 走「写警告」）
- 对改动的源码文件运行 `pre-commit run --files <文件列表>`；失败则记下完整输出与 file:line。
- 若 `gdsl/` 有改动：运行 `powershell -File gdsl/test.ps1`；失败则记下输出。

## 4. 决策与执行
全部通过时：
- `git add <逐个列出文件>`（显式列出，不用 -A）。
- `git commit -m "<信息>"`。信息按 CONTRIBUTING.md：祈使句、首字母大写、首行 <72 字符、可带区域前缀（如 `Core:`）。
- `git push origin HEAD`（当前分支 `4.7-local` 无 upstream，用 `HEAD` 推同名远程分支）。
- 完成后输出一句话摘要：提交 hash + 推送结果。

任一失败时：
- 用 edit 工具**追加**一段到 `.kilo/review-warnings.md`：`## <ISO时间戳>` 标题 + 逐条 `- 文件:行 — 原因 — 证据`。
- 不提交、不推送。
