# Feature Specification: LLM 侧接口 — 正确性 + 可学性（跨层）

**Feature id**: `010-llm-interface`
**Created**: 2026-08-30
**Status**: Draft（forward-spec — 大部分待实现）
**Input**: 复盘 Era 16 的两个目标：① DSL 正确性高、容易验证；② LLM 生成快、学习容易。

## 目标（两条硬约束，不是口号）

1. **DSL 正确性高、容易验证**：机器能在「合法/非法」之间划一条可证的线，坏输入必被拦、且拦得住。
2. **LLM 生成快、学习容易**：LLM 用最少示例学会语法，首轮就大概率合法，错了能靠报错在 1–3 轮内改对。

这是 `GODOT_LLM_DSL_DESIGN.md`「可验证 > 可表达」的落地 spec。之前 001–009 只 spec 了编译器内部，
没 spec LLM 面对的接口层；本 spec 补上，并把两条目标拆成可证伪的 law 与可度量的 metric。

## 目标 1：DSL 正确性高、容易验证

### Primary User Story
作为验证器，我要对任意 DSL 输入给出确定答案——合法则通过，非法则拒绝并指出「哪里、哪个符号、
期望什么」，使「这个 DSL 能不能被 LLM 用」成为一件可测的事，而不是一句口号。

### Acceptance Scenarios（falsifiable bar）
1. **Given** 任意合法程序，**When** parse→typecheck→codegen，**Then** 全链通过且输出确定
   （round-trip，property-based：一个反例即违约）。
2. **Given** 任意非法程序，**When** 校验，**Then** 被拒绝且 `err` 非空、含位置与违规符号名
   （不是空泛的 "parse error"）。
3. **Given** 同一合法输入，**When** codegen 两次，**Then** 逐字节一致。
4. **Given** 一份声明式 JSON 里 connection 引用了不存在的节点，**When** `scene_from_json`，
   **Then** 被拒绝——这是当前 schema 校验的洞（只查结构不查语义），本 spec 要求补上。

### Edge Cases
- 引用不存在的节点 / 未声明的资源路径。
- 报错信息缺定位（行/列）或缺符号名——本身算违规。

### Functional Requirements
- **FR-001**（law）: 合法程序 round-trip 且 typecheck 通过。反例即违约。
- **FR-002**（law）: 非法程序拒绝且报错含定位 + 符号名 + 期望/实际。缺任一项算违规。
- **FR-003**（law）: codegen 确定性（已在 003/008 落地，本 spec 在接口层重申）。
- **FR-004**（law，新增 gap）: 语义校验——声明式层 connection 引用的节点必须存在、资源路径可解析；
  当前 002 只做结构校验，这是要补的洞。
- **FR-005**（metric）: validator mutation score ≥ 70%——坏输入必须被测试套件抓出来，证明校验器不是摆设。

## 目标 2：LLM 生成快、学习容易

### Primary User Story
作为 LLM，我要用最少示例学会这个 DSL，首轮输出大概率合法，错了能靠报错在几轮内改对——
「容易学」和「生成快」是度量的，不是形容词。

### Acceptance Scenarios
1. **Given** 一份 few-shot 示例（1–2 个），**When** LLM 首轮生成，**Then** 通过 schema+typecheck
   的比例达到可测目标（first-try valid rate）。
2. **Given** 一份非法输出，**When** LLM 收到报错，**Then** 在 1–3 轮内改到合法
   （iterations-to-valid）。
3. **Given** 任意报错，**When** 人读它，**Then** 一眼知道错在哪、错什么、怎么改——报错本身
   是可测的，不是「能不能」的问题。

### Edge Cases
- ontology 动词太少导致想表达的表达不了 → LLM 学习「容易」但表达「无能」，反而落空。见 FR-010。

### Functional Requirements
- **FR-006**（metric）: first-try valid rate——LLM 首轮输出通过校验的比例。趋势项，不设门，但要测。
- **FR-007**（metric）: iterations-to-valid——非法→合法的反馈轮数，目标 1–3（对齐 SOP §1.1）。
- **FR-008**（metric）: few-shot 数量——教会 LLM 写合法 DSL 所需示例数，目标 1–2；超了说明语法不友好。
- **FR-009**（law）: 报错可定位、可理解——每条错误含位置 + 违规符号 + 期望/实际。这是 machine-checkable。
- **FR-010**（morality，设计 §7 最大风险）: ontology 覆盖边界——固定动词集必须够写真实玩法；
  新增 effect 动词须先过 review（继承 005 的 morality），不静默扩展。覆盖是否足够，用真实玩法样本压测，
  不能只靠纸面推断。
- **FR-011**（forward）: 收敛闭环——playtest 反馈喂回 LLM 的自动纠错环（ScriptDoctor 先例证明的
  「编译错 + playtest 反馈」环的后半段）。009 只做到「加载真场景」，这半段是新增，未建。

## Key Entities

- **报错规范**：定位（file:line:col）+ 符号名 + 期望/实际。当前 004/005/006 的 `err` 是自由文本，无契约。
- **few-shot 示例集**：教会 LLM 写合法 DSL 的最小示例，需单独维护。
- **收敛测试集**：真实玩法样本，用来压 ontology 覆盖（FR-010）与度量 FR-006/007/008。

## Review & Acceptance Checklist
- [x] 每个 FR 可测（law 可证伪 / metric 可度量 / morality 靠 review）
- [x] FR-004 语义校验 gap 已落地（connection 的 from/to 引用不存在节点 → 拒绝；`scene_from_json` 校验，测试 `[GDSL] Reject connection referencing a nonexistent node`）
- [x] FR-009 报错契约已落地（parser 全部 reject 路径带 `line N:` 定位 + 点名违规符号；typecheck/effect 已点名符号。测试 `[GDSL] Parser error carries a line number`、`[GDSL] Typecheck error names the offending symbol`）
- [ ] FR-006/007/008 的度量 harness 已建（当前无 LLM 收敛测试）
- [ ] FR-011 收敛闭环已建（当前无 playtest 反馈，依赖 009 引擎集成）
