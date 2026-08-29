# JOURNEY.md — godot 引擎 fork 的引导阶段记录

> 本文件记录「人怎么决策、纠正、砍方向」与「AI 怎么执行、证伪、诚实报告」的时间线。
> 列名：`ME = 用户`，`YOU = AI`。日期格式 `YYYY-MM-DD`。
> 当前阶段：**引导期**——仓库刚 fork + OpenWolf 刚落地，尚未进入真实引擎开发。历史很薄，如实记录，不灌水。

## 风险与待办（roll-up，非替换——各时代的行内风险/待办仍在原表里）

- **[Era 1·起步] 浅克隆 + fork 远端**：`git rev-parse --is-shallow-repository` = `true`，仅见一个 commit `f6ab5db`；remote 是 `yhyu13/godot`。**不能假设能 push 上游 `godotengine/godot`**，PR 走 fork 流程。
- **[Era 4·OpenWolf] web-app 模板与 C++ 引擎错配**：`.kilo/command/{reframe,security-audit}.md`、`.claude/commands/*`、`.wolf/OPENWOLF.md` 的 `designqc`/`reframe`/`npm audit` 都是 React/Tailwind/web 套路。agent 照做会落空（本仓库无 `package.json`/dev server）。待确认：裁剪成 Godot 适用，还是接受不适用。
- **[Era 4·OpenWolf] 记忆库全空**：`.wolf/{cerebrum,memory,STATUS}.md`、`buglog.json`、hippocampus/token-ledger 均 15:54:07 初始化为零。TODO：真实开发开始后按 OPENWOLF.md 协议回填，否则 OpenWolf 形同虚设。
- **[Era 4·OpenWolf] anatomy.md 对 C++ 文件价值低**：`file_count=513, hits=0`，多数描述只是版权首行（如把 `CLAUDE.md` 标成「OpenWolf」）。RISK：agent 依赖 anatomy 描述会读到无意义文本。
- **[Era 2-3·AGENTS] 两份指令文件会漂移**：`AGENTS.md`（紧凑）与 `CLAUDE.md`（更详）内容重叠（构建/测试/架构/贡献约定）。TODO：后续改动若只更新一份，另一份会过期。

---

## Era 1 — 起步：Godot 引擎 fork（浅克隆）落地

| ME | YOU |
|---|---|
| 建立了 `yhyu13/godot` 的 fork，checkout 到 `4.7` 分支（浅克隆，只拉了一个 commit）。 | 确认仓库身份：Godot Engine 源码，`version.py` = 4.7.3-rc，分支 `4.7`，remote 指 `yhyu13/godot` 而非上游。 |

- 事实锚点：`git log` 仅见 `f6ab5db` "Merge pull request #122593 from akien-mga/4.7-disable-physics-nav-3d"（Rémi Verschelde，2026-08-20）。
- 此时仓库根目录**没有** `AGENTS.md`，但有一份未跟踪的 `CLAUDE.md`（79 行，内容准确）。

---

## Era 2 — AGENTS.md 首轮：把构建体系压成可核事实

| ME | YOU |
|---|---|
| 「Create or update `AGENTS.md` for this repository.」并立下两条纪律：「Prefer executable sources of truth over prose.」「When in doubt, omit.」 | 读了 `README`/`pyproject.toml`/`.pre-commit-config.yaml`/`version.py`/`CONTRIBUTING.md`/`SConstruct`/`tests/`，逐条给 `file:line` 锚点，写成 47 行 AGENTS.md。 |

- 写进 AGENTS.md 的高信号事实：SCons 而非 CMake/Make；`target` ∈ editor/template_debug/template_release（`SConstruct:164`）；Windows 默认 MSVC、`use_mingw=yes` 换 MinGW（`detect.py:179-182,227`）；`tests=yes`/`compiledb=yes`/`ninja=yes`/`dev_mode=yes`；`module_<name>_enabled=no`（`SConstruct:485`）；产物命名 `bin/godot.<platform>.<target>[.dev].<arch>` 与 Windows `.console.exe` 双产物（`SConstruct:1034-1051,1157`）；doctest `--test` 过滤（`test_main.cpp:117-123`）；GLSL 用独立 clang-format 配置；架构分层 + servers 模式 + 不碰 `thirdparty/` + 生成文件不手改；改脚本 API 必更新 `doc/classes/*.xml`（`--doctool`）。

---

## Era 3 — 纠错：`platform` 不是「必填」，是「自动探测」

| ME | YOU |
|---|---|
| （未直接指出，但任务隐含「信可执行源」的标准在起作用。） | **先写错了**：首版 AGENTS.md 写「`platform` 必填、缺省即报错」——这是从注入内容里继承的过时说法。随后对拍 `CLAUDE.md` 与 `SConstruct:383-425`，发现 4.7 已改为从宿主 OS 自动探测（win32→windows、darwin→macos、linux→linuxbsd），只有探测不到或跨平台才需显式传 `platform=<p>`。**立刻修正**。 |

- 这是本阶段唯一一次实质纠错，也是「散文说法 vs 可执行源」冲突时以 `SConstruct` 为准的一次落地。
- 教训已固化：**过时的注入内容会以「事实」的面目混进来，交叉比对可执行源是唯一防线。**

---

## Era 4 — OpenWolf 落地：上下文管理系统注入

| ME | YOU |
|---|---|
| 在仓库里初始化了 OpenWolf（`15:54:07`），并让它覆盖 `.kilo/`/`.claude/`/`.cursor/`/`.codex/`/`.opencode/` 五套 agent 配置。 | 排查后确认：`.wolf/` 全套状态（memory/cerebrum/buglog/hippocampus/token-ledger）**全部零初始化**；`AGENTS.md` 与 `CLAUDE.md` 被自动追加了 `<!-- openwolf:begin -->` 引用块；`anatomy.md` 扫描到 513 个文件、0 hits。**诚实发现：OpenWolf 是 web-app 通用模板，与本 C++ 引擎错配**（designqc 截图、reframe UI 迁移、npm audit 都无处可施）。 |

- 事实锚点：`_scan-state.json` `git_head=f6ab5db28…`、`file_count=513`；`hippocampus.json` `total_events=0`；`token-ledger.json` `total_sessions=0`。

---

## Era 5 — AGENTS.md 复验：零改动交付

| ME | YOU |
|---|---|
| 再次下达同一个「Create or update `AGENTS.md`」任务（复验性质）。 | 逐条复验上一轮的全部锚点（`compiledb`/`ninja`/`tests`/`.console.exe`/doctest/GLSL/`--doctool`），确认代码库未变（仍 `f6ab5db`）、AGENTS.md 无漂移、OpenWolf 记忆库为空、无可合并的「项目实学」。**结论：不改，而非为交付而改。** |

- 诚实的「零改动」也是结果：文件已满足「每行都是 agent 不看会踩坑」的标准，再动就是破坏。

---

## Era 6 — 当前：dump journey

| ME | YOU |
|---|---|
| 「dump to journey」——要求把历史沉淀成可复用的 vibe-coding 经验。 | 从 `.wolf/` 记忆库（全空）+ `kilo_local_recall`（仅 1 条会话）+ git（单 commit）重建时间线，如实写成这份 JOURNEY.md。 |

---

## 这个项目如何教 vibe coding with AI

### 人的工作（decide, correct, kill）

- **把纪律写进任务本身**：两次 AGENTS.md 任务都自带「Prefer executable sources of truth over prose」「When in doubt, omit」「improve in place rather than rewriting blindly」——这些不是口头要求，是写死的验收标准，直接决定了我对拍 `SConstruct` 并抓到过时说法（Era 3）。
- **用复验代替追加**：第二次下达同一任务（Era 5）不是为了加内容，而是逼我逐条复查；结果是诚实的「零改动」而不是无意义的增补。

### AI 的工作（instrument, falsify, report honestly）

- **交叉比对可执行源，抓到自己的错**：Era 3 里「`platform` 必填」是我先写进去的错，靠 `CLAUDE.md` vs `SConstruct:383-425` 对拍才推翻。AI 的价值在于敢于报告「我先写错了」。
- **报告不适用，而不是硬套**：Era 4 识别出 OpenWolf 的 web-app 模板与本 C++ 引擎错配，明确说「designqc/reframe/npm audit 无处可施」，而不是假装能用。
- **空就是空**：Era 5 明确「记忆库全空、无项目实学可合并」，不为凑篇幅编造历史。

### 可复用的规则

1. **注入内容 ≠ 事实，可执行源才是**：任何从上下文/模板里顺来的说法，先到 `SConstruct`/`detect.py`/配置里对拍再落笔（Era 3 的 `platform` 纠错）。
2. **逐条给 `file:line` 锚点**：AGENTS.md 每一条都钉在源码行号上，复验时一眼可查、不会漂移（Era 2 → Era 5 全程可复验）。
3. **「零改动」是合法交付**：复验确认无误时，不改比硬改更诚实（Era 5）。
4. **识别模板错配要明说边界**：发现工具/模板与项目类型不符时，报告「不适用」并给出原因，而不是让下个 agent 照做踩坑（Era 4）。
5. **记忆库为空时要直说**：重建历史时 `.wolf/` 全空、`kilo_local_recall` 只有 1 条会话，就写一份诚实的引导期记录，而不是编一段「十时代史诗」（Era 6 全程）。

### 一句话总结

人把「信可执行源、宁可省略」的纪律写进任务并反复验收，AI 负责把散文压成带锚点的事实、在对拍中揪出自己的过时假设、并如实报告「零改动 / 不适用 / 记忆为空」——**人立标准，AI 保真。**
