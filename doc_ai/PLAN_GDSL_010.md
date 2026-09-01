# PLAN — 010 LLM 收敛度量：gdsl 到底「LLM 友好」吗？

> 一句话：拿真 LLM（Claude Code headless，`-p` print mode）跑一组游戏配方任务，让它产出 `.gdsl`，
> 用 `gdslc logic`（parse+typecheck）判「有效/无效」，量出 first-try rate / iterations-to-valid / few-shot 敏感性。
> 这是 Era 16 说「LLM 友好目前未证」的直接度量——不是感觉，是数字。

## 0. 目标（persist，全程不窄化）

回答：**LLM 是否能用 gdsl 写配方？** 用可复现度量代替「应该很友好」的主观判断。

- 度量（4 个）：
  1. **first-try rate** = K 个 few-shot 下，LLM 第一次输出就通过 `gdslc logic` 的任务占比。
  2. **iterations-to-valid** = 拿 typecheck 报错让 LLM 改，到第一次有效所需的兜底次数（1 = 首轮就过；≥2 = 改了几次；n/a = 没收敛）。
  3. **valid rate** = 在设定兜底上限内最终达到有效的任务占比（有的任务可能永不收敛——这本身是发现）。
  4. **few-shot 敏感性** = K=0 vs K=1 vs K=2 时 first-try rate 的差异（反过来衡量「语法是否自解释」）。

## 1. 什么叫「有效」（钉死，不能含糊）

`gdslc logic <tmp.gdsl> <tmp.c>` 返回码 = 0（parse + typecheck 全过）。这是「编译期有效」。
**不**把「运行时正确」当有效——那需要引擎 + 场景断言，是 009 的事。本轮的 bar 就是「LLM 产出到可编译的
.gdsl」，这正是 reject-not-retry 哲学承诺的那一步。

## 2. 设计（测量公正性优先）

- **冷测**：Claude 只看到「gdsl 语法参考 + K 个 few-shot 示例 + 任务描述」，**不访问仓库/不读 Godot 源码**
  （`--allowedTools ''` 禁工具，`--max-turns 1` 纯生成，`--output-format json` 拿 cost）。这样测的是
  「语言本身 LLM 友不友好」，不是「LLM 擅不擅长扒源码」。
- **few-shot 来自仓库真实示例**（`named.gdsl`/`self_rule.gdsl` 由我抽取成合法例子），不是编造的。
- **任务集覆盖语法面**：基础 type+state、guard+effect、float、string、Named(object ref)、target 跨参与者、emit。
- **迭代反馈**：失败时把 `gdslc` 的 typecheck 报错追加进提示，让 LLM 改。这是闭环（ScriptDoctor 式），不是
  单纯计数失败。
- 失败**不重试同一 prompt**（否则计成 1 次迭代但无效），每次迭代都是「报错→改」。

## 3. 实现（一个 Python 脚本，零引擎依赖）

`gdsl/toolchain/llm_conv_bench.py`：
- `TASKS`：5 个配方任务（辞 → 期望 .gdsl 覆盖某语法点）。
- `GRAMMAR`：gdsl 语法参考（从 parser/typecheck 对拍写出）。
- `FEWSHOT`：`{0:[], 1:[named.gdsl], 2:[named.gdsl, self_rule.gdsl]}`。
- 每 (task, K)：循环 ≤ `MAX_ITER`（首轮 3）：
  1. 拼 prompt（语法 + few-shot + 任务 + 「只输出 .gdsl，无工具无文件」）。
  2. `claude -p <prompt> --allowedTools '' --max-turns 1 --output-format json`。
  3. 解析 JSON → `.result` 文本 + `.total_cost_usd`。
  4. 抽 `.gdsl`（剥 markdown fence / 代码块）。
  5. `gdslc logic` → rc=0 有效 / rc=1 无效（拿 stderr 报错）。
  6. 有效 → 记 iterations、break；无效 → 报错追加进提示，重来。
- 聚合每 K 的 4 个指标 + 总 cost/耗时，打印表格。

## 4. 成功标准（本轮）

1. Harness 端到端跑通（任务 → claude → .gdsl → gdslc → 迭代）。
2. 产出 4 个指标（初跑 K=0 与 K=1，任务≤5），数字可复现。
3. `gdslc` 无回归（96/397 仍绿）。
4. 诚实：报告「K=0 first-try」「K=1 first-try」「平均 iterations」「已有任务有无永不收敛」，不粉饰。

## 5. 风险 / 边界（诚实标注）

- **费用/耗时**：~18-30 次 `claude -p`，每次 ~10-25s + 少量 USD。首跑限制 3 任务 × K∈{0,1} × ≤3 迭代 ≈ ≤18 calls。
- **LLM 一致性**：非确定性，指标会漂移；同任务跑 1 次说不了「稳定」，只报「该次跑」的数字（可复跑取均值）。
- **prompt 偏见**：语法参考写得好坏会影响结果。我按 parser/typecheck 对拍写，尽量中肯；仍属「用这个语法参考」
  条件下的测量。
- **不碰引擎/不碰 009**：纯 gdsl/ 工具 + 临时目录，零引擎改动。

## 6. 交付物

- `gdsl/toolchain/llm_conv_bench.py`（harness）。
- 一轮真实度量（数字）。
- `doc_ai/PLAN_GDSL_010.md`（本文件）。
- JOURNEY Era + cerebrum 决策。
