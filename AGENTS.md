# AGENTS.md

Godot Engine source (C++17, MIT), version 4.7.3-rc, branch `4.7`. Origin remote is the fork `yhyu13/godot` — do not assume push access to upstream. The long-form contributing guide is `CONTRIBUTING.md`; this file only adds what an agent would otherwise guess wrong.

## Build (SCons — not CMake/Make)

- `scons platform=<p> target=<t> -jN`. `platform` defaults to auto-detecting the host OS (`windows`/`linuxbsd`/`macos`); pass it explicitly to cross-build or target a mobile/web platform. Values: `windows`, `linuxbsd`, `macos`, `android`, `ios`, `web`, `visionos` (`SConstruct:383-425`).
- `target` ∈ `editor` (default) / `template_debug` / `template_release` (`SConstruct:164`).
- On Windows the default compiler is MSVC; use `use_mingw=yes` for MinGW.
- Flags that change behavior:
  - `dev_build=yes` → `DEV_ENABLED`, assertions on.
  - `tests=yes` → compiles the doctest unit-test code (`SConstruct:240`, `SConstruct:1238`).
  - `compiledb=yes` → emits `compile_commands.json` (required for clang-tidy/clangd).
  - `ninja=yes` → ninja backend; `dev_mode=yes` = `verbose=yes warnings=extra werror=yes tests=yes strict_checks=yes`.
  - `module_<name>_enabled=no` disables an optional module (`SConstruct:485`); `modules_enabled_by_default=no` disables all but explicitly-enabled ones.
- Output goes to `bin/` with an encoded name `godot.<platform>.<target>[.dev].<arch>` (`SConstruct:1034-1051`). On Windows there is also a `.console.exe` twin (`SConstruct:1157`): the plain `.exe` is a GUI app with no stdout, use the `.console` one to see logs.

## Tests (doctest)

- Build with `tests=yes`, then run the binary with `--test`. doctest CLI flags pass through (`tests/test_main.cpp:117-123` strips only `--test`):
  - `bin/godot.windows.editor.x86_64.exe --test --test-case="String*"` — filter by case.
  - `--test --test-suite="core/math"` — filter by suite.
- Unit tests live in `tests/`, mirroring the `core/`/`scene/`/`servers/` layout; register via `TEST_CASE`/`TEST_SUITE` from `tests/test_macros.h`.
- GDScript integration tests are separate: `modules/gdscript/tests/scripts/` (see `modules/gdscript/tests/README.md`), run by a dedicated runner, not the doctest binary.

## Lint / format (pre-commit)

- `pre-commit run --all-files` is the gate. `clang-format` for C/C++/Java; **GLSL uses a different config** `misc/utility/clang_format_glsl.yml` (`.pre-commit-config.yaml:26-30`). Python/SCons files use `ruff` + `mypy`; `codespell` checks spelling.
- `clang-tidy` is `manual`-stage only and needs an up-to-date `compile_commands.json`:
  ```bash
  scons platform=windows target=editor compiledb=yes
  pre-commit run --hook-stage manual clang-tidy
  ```

## Architecture (layered; most edits go to one layer)

- `core/` (Variant, String, Object, math, ClassDB bindings) → `scene/` (Node/SceneTree, resources, GUI) → `servers/` (singleton backends: rendering/physics/audio/navigation) → `editor/` → `modules/` (optional features, each registers via `modules/SCsub`) → `drivers/` + `platform/` (OS ports) → `main/` (entry point).
- The **servers pattern** is Godot's idiom: scene nodes are thin frontends; real state and work live in `servers/`. New systems are typically split frontend-node + server backend.
- `thirdparty/` is vendored — never edit it directly.
- Generated files (`.gen.h`, `register_module_types.gen.cpp`, `modules_enabled.gen.h`, `*.compat.inc`) are produced at build time — don't hand-edit them.

## Contribution rules that are enforced (not advisory)

- Commit messages: imperative, capitalized, first line <72 chars, optional area prefix (e.g. `Core: Fix …`); extended description wrapped at 80. Fixes use GitHub closing keywords in the PR **description**, not the title (`CONTRIBUTING.md:100-142`).
- **Any PR adding/changing script-exposed methods/properties/signals must update `doc/classes/*.xml`** — regenerate with the compiled binary's `--doctool`, then hand-fill descriptions (`CONTRIBUTING.md:148-158`; `main/main.cpp:716`).
- Feature proposals go to `godotengine/godot-proposals`, not this repo's issue tracker (`CONTRIBUTING.md:43-47`).
- C++ style follows Godot's usage guidelines and `.clang-format`; `doc/tools/make_rst.py` / `doc_status.py` enforce doc coverage.
- **Never claim a crash root cause without a symbolicated stack.** A crash/teardown segfault (e.g. the GDExtension-exit EXIT 139) must be diagnosed from a real call stack or a control experiment (ab/对照) — not from source-path reasoning. Verified against `.wolf/cerebrum.md` Do-Not-Repeat `[2026-08-31]`: asserting a teardown root cause from inference was wrong, and the accompanying `p_is_static` change broke the test suite. Fix the cause, not the symptom; state the evidence gap honestly ("no stack yet") instead of presenting an unverified hypothesis as a finding.

## Module teaching docs (144, in Chinese)

Every one of Godot's 144 subsystems (`core/`, `scene/`, `servers/`, `editor/`, `modules/`, `drivers/`, `platform/`) has a Chinese teaching doc — conclusion-first, source-anchored (`file:line`), with mermaid diagrams and 口诀/练习/自测. Index: [`docs/INDEX.md`](docs/INDEX.md) (grouped by layer, clickable); writing spec: [`docs/DOC_SPEC.md`](docs/DOC_SPEC.md). Check the index before reading a module's source — the doc may already answer it.

## GDSL / LLM-DSL project (spec-first, in-repo docs)

The LLM-friendly DSL work lives **completely inside this fork** — its design, specs, and team rules are in-repo and are the source of truth for anything touching `gdsl/`. **Read the relevant spec before writing; read `.wolf/cerebrum.md` before generating code** (it carries the Decision Log, Do-Not-Repeat lessons, and ABI walls).

- **Specs: [`specs/`](specs/)** — 10 slice specs (`001-json` … `009-engine-integration`, `010-llm-interface`), one per component, run in order; `009` is active (`specs/009-engine-integration/spec.md`). `010-llm-interface` is the forward-spec for "is the DSL actually LLM-friendly" (first-try rate / iterations-to-valid, via `gdsl/toolchain/llm_conv_bench.py`).
- **Design: [`doc_ai/GODOT_LLM_DSL_DESIGN.md`](doc_ai/GODOT_LLM_DSL_DESIGN.md)** — the two-layer architecture (declarative JSON→`.tscn` via `ResourceFormatLoaderText`; logic → GDExtension `ptrcall`); source-anchored; the "可验证 > 可表达" thesis. Read before touching `gdsl/` codegen.
- **AI-native audits:** [`doc_ai/LLM_UNFRIENDLY_DESIGNS.md`](doc_ai/LLM_UNFRIENDLY_DESIGNS.md) (8 Godot traits that are anti-LLM + counters) and [`doc_ai/LLM_READABILITY_AUDIT.md`](doc_ai/LLM_READABILITY_AUDIT.md) (binary layer; key conclusion: don't convert the 107 binaries — use `extension_api.json` / `gdextension_interface.json` / `doc/*.xml`).
- **Process: [`doc_ai/SOP_TDD_AI_TESTING.md`](doc_ai/SOP_TDD_AI_TESTING.md)** — law/metric/morality three-tier, seam cards, red-before-green, hard Definition of Done (§4.4), anti-drift (§4.5). Operating procedure for `gdsl/` slices and AI follow-through.
- **Long-run autonomy: [`doc_ai/GDSL_LONG_RUN.md`](doc_ai/GDSL_LONG_RUN.md)** — rules for continuous/autonomous ~8h agent work (contract + state + resumable; Gate-gated exploration; parallel ≫ serial).
- **Status: `.wolf/STATUS.md`** (single source of truth for done/next); day-plans in `doc_ai/PLAN_GDSL_8H.md`, `PLAN_GDSL_010.md`, `PLAN_GDSL_DIR5.md`.

The GDSL kernel is a **standalone** C++17 tree (`gdsl/`) with its own doctest harness — `cd gdsl && powershell -File test.ps1` runs the whole suite in seconds. **Do not rebuild the engine to test `gdsl/`**; engine integration is only slice S6/`009`.

## Parallel agent work (this repo)

This repo has a heavy C++ build and a large tree. **Never use git worktrees here — the cost explodes.** Each worktree needs its own full `scons` editor build (15–45 min and several GB in `bin/`, plus its own `compile_commands.json`). Don't create one to isolate file edits, and don't create one to isolate a build — it is never worth it here.

Instead:

- Each parallel agent owns a disjoint set of paths (file-ownership partitioning). The layered architecture plus the isolated `gdsl/` tree make this easy — `gdsl/*`, `specs/*`, `doc_ai/*`, `.wolf/*` are separable and rarely collide.
- Never `git add -A`; stage only the exact files you own.
- Commits are the orchestrator's job. Workers generally do not commit; a successful, reviewed cycle may get a reviewed local commit only on explicit authorization. If a worker would need an independent commit, it waits and hands off — never sweeps another agent's staged changes into its own commit.
- Only slice `009` needs an engine build; every other `gdsl/` slice runs the `test.ps1` harness in seconds. Do not trigger `scons` for them.
- If two agents would genuinely need the engine build at once, serialize it — one engine build at a time, and sequence the `scons`-requiring slice after the others. Never run concurrent `scons` in this repo.

<!-- openwolf:begin -->
# OpenWolf

@.wolf/OPENWOLF.md

This project uses OpenWolf for context management. Read and follow .wolf/OPENWOLF.md every session. Check .wolf/cerebrum.md before generating code. Check .wolf/anatomy.md before reading files.
<!-- openwolf:end -->
