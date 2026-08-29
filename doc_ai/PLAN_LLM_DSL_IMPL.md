# PLAN — Godot LLM DSL 实现 · 第 1 阶段（编译器内核）

> 关联设计：`GODOT_LLM_DSL_DESIGN.md`（两轮 critic 已收敛，零 blocking）。
> 本计划回答三件事：切哪几片、第一片怎么切（seam + DoD）、编译器住哪怎么测（两个待确认的架构分叉）。

**结论：本阶段只做「纯函数编译器内核」，用最快的测试循环；引擎集成（module 注册 / GDExtension 绑定 / 真机跑场景）留到内核稳定后。第一片建议从 §8 的「parser」改成「声明式 JSON→.tscn」——理由见自评。**

## 0. 持久化目标（整轮不变，每轮重注入）

把两层 DSL 落地为可编译、可测的编译器：输入声明式 JSON + 逻辑 `.gdsl`，输出 `.tscn`/`.tres` 与 C(GDExtension)。**本阶段里程碑 = 声明式层端到端：一份 JSON 场景配方 → 生成可被 Godot 加载的 `.tscn`。**

## 1. 切片分解（垂直，每片一个 red→green）

| # | 切片 | 输入 → 输出 | 依赖 | 风险 |
|---|---|---|---|---|
| S1 | 声明式 codegen | JSON 场景 → `.tscn` 文本 | 无 | 最低，tracer bullet |
| S2 | 声明式 schema 校验 | 非法 JSON → 拒绝 | S1 | 低 |
| S3 | 逻辑 parser | `.gdsl` → AST | 无（纯函数） | 中 |
| S4 | 逻辑 typechecker | AST → typed IR | S3 | 高（也是 ptrcall 免 UB 闸门） |
| S5 | 逻辑 codegen | IR → C(GDExtension) | S4 | 高 |
| S6 | 引擎集成 | `.gdextension` 跑真场景 | S5 | 高 |

## 2. 第一片 S1 的 seam 卡

```
Feature: 声明式场景配方转 .tscn 文本
Seam:    gdsl::emit_tscn(const SceneSpec&) -> String
Inputs:  {root:{type:"Node2D",name:"Main"}, children:[{type:"Sprite2D",name:"icon",props:{position:{x:10,y:20}}}]}
Expected: 一段含 "[gd_scene format=3]"、"[node name=\"Main\" type=\"Node2D\"]"、子节点与 position 属性的 .tscn 文本
Tag:     [GDSL] Emit minimal tscn
```

成功标准（DoD，照 SOP §4.4）：测试先红（失败断言，非编译错）→ 最小实现 → 绿 → 测试文件红绿间逐字节不变 → 确定性（同一输入逐字节同一输出）。

## 3. 两个开放决策（写码前必须定死，直接影响所有后续文件的落点）

**D1 — 编译器住哪 + 测试循环。**
- 方案 A：新引擎模块 `modules/gdsl/` + 复用引擎 doctest（`scons tests=yes` → `--test`）。合 SOP 与 GDScript 先例，但每次 red-green 要重链整引擎，分钟级，SOP §1.1 的 cycle-time 目标会被打穿。
- 方案 B：独立工具目录 + 快速 standalone 测试（只编 parser/codegen + 一个 doctest main，秒级）。纯函数阶段最优，后期再把内核接进模块。
- **推荐 B**：S1–S5 全是纯函数、不依赖引擎运行时，用引擎重链去测它们是浪费；S6 再走模块集成。

**D2 — 第一片选哪个。**
- 方案 A：S1 声明式 JSON→.tscn（**推荐**）。产出可加载的可见场景，不需 GDExtension/ptrcall 任何机制，是真正的 tracer bullet。
- 方案 B：S3 逻辑 parser（§8 原计划）。纯函数没错，但 AST 要等 S4/S5 才有用，前几片看不到端到端产物。

## 4. 自评（self-critique，写码前一次）

1. **§8「parser 先行」是我自己在设计文档里拍的，这里推翻它**：parser 的 AST 是中间产物，单个 slice 拿不出用户可见结果；S1 的 `.tscn` 是真实可加载产物，能更早暴露「JSON 模型 ↔ tscn 语义」的映射错误。tracer bullet 优先。
2. **D1 的 B 方案违背「复用 repo 测试约定」**，代价是后期集成时要把 standalone 测试迁回 `tests/`。接受，因为 cycle-time（SOP §1.1 的 metric）优先于约定一致性；且纯函数代码本身可移植，迁移成本是机械的。
3. **风险未覆盖**：JSON 解析用哪个实现（repo 的 `core/io/json.cpp` vs standalone 的 json 库）在 S1 没定。若走 D1-B，得带一个 header-only JSON 解析；若走 D1-A，直接用 `core/io/json.cpp`。这条挂 D1 一起定。
4. **effect ontology 边界仍未定**，但它只影响 S4+，不阻塞 S1；明确推迟，不在本片展开（避免过度设计）。

## 5. 待用户拍板

D1（编译器住哪/怎么测）、D2（第一片）。定完即可进入 S1 的 red→green。
