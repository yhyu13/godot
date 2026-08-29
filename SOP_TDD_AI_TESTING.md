# SOP — Test-Driven Development with AI Agents in Godot

> Scope: the `godot` engine repository (C++17, SCons, doctest). Version 4.7 branch.
> Purpose: a single operating procedure for (1) picking metrics, (2) choosing what
> to test, (3) writing a test correctly, and (4) driving an AI agent to actually
> finish the red→green→refactor loop instead of drifting or declaring victory early.
> This is a *process* doc; it complements `CONTRIBUTING.md` and the `tdd` skill.

---

## 0. TL;DR

1. **Metrics** = process (red authenticity, cycle time, iterations-to-green) +
   quality (mutation score, coverage delta, flake rate) + delivery (finish rate,
   token cost per green test, regression escape).
2. **What to test** = behavior at pre-agreed public seams, ordered by testability:
   `core/` math/string/templates/variant first, then `servers/`, then `scene/`,
   then `modules/`. Never test rendering internals, `thirdparty/`, or generated files.
3. **How to write** = one `.cpp` under `tests/<layer>/`, `TEST_FORCE_LINK` +
   `TEST_CASE("[Area] behavior")`, `CHECK`/`REQUIRE`, build `tests=yes`, run `--test`.
4. **How to make AI finish** = freeze the seam first, force red before green,
   vertical slices, a hard "definition of done" gate, and a re-injected objective
   so the agent cannot claim completion while the build is red.

---

## 0.5 The philosophy — law vs. metric vs. morality（三层：法律 / 度量 / 道德）

TDD as a system only works if every rule is sorted into exactly one of three
tiers and the tiers are kept separate. The failure mode is conflating them:
turning a soft preference into a hard gate produces false reds; turning a hard
gate into a trend number means it stops being enforced.

| Tier | 中文 | What it is | Enforced by | Examples already in this doc |
|---|---|---|---|---|
| **Law** | 法律条文 | Few, binary, machine-checkable, falsifiable, zero-false-positive gates. "Must pass ALL laws to ship." | CI / build breaks | §4.4 DoD: exit 0, targeted green, no regression, test byte-identical between red→green |
| **Metric** | 度量仪表盘 | Trended numbers that *inform* but do **not** gate. | Dashboard / report | mutation score, coverage delta, cycle time, token cost per green test |
| **Morality** | 道德约束 | Process principles about judgment; soft, exceptions allowed. | Human review | `.wolf/identity.md` Constraints; "explain before architectural change" |

**The criterion for "can this be a law" is falsifiability（可证伪）.** A rule
qualifies as law only if a single counterexample breaks it. That is exactly why
*properties/invariants* — not examples, not preferences — are the natural laws:
`reverse(reverse(x)) == x` is a law (property-based testing: run N random
inputs, one counterexample = guilty). A behavioral example `abs(-5) == 5` is a
*precedent*（判例）, not a law. "Explain before architectural change" is a
*morality* — it is about judgment and cannot be quantified.

**Two failure modes of over-legislating（为什么「把所有 prop 都升成法律」会失败）:**

1. **False precision（假精度）** — assigning a number to a preference ("should be
   fast" → "must be <5ms"). The threshold is arbitrary; a slower CI machine turns
   it red; people then disable the law.
2. **Goodhart's law（古德哈特）** — "when a measure becomes a target, it ceases to
   be a good measure." Gate on raw coverage and agents write line-touching
   no-assertion tests to hit the number. This is why §1.4's interpretation rule
   says: never gate on raw line coverage; weight mutation score higher.

**Evidence from this repo:**

- A hard law propagates through autonomous agents: `docs/DOC_SPEC.md` made
  "grep before writing, never fabricate a symbol" a hard rule, and 144
  autonomous doc-writing agents enforced it — they repeatedly corrected wrong
  hints (nanosvg→ThorVG, MultiplayerReplicator→SceneReplicationInterface,
  InputFilter absent). A law written into the contract gets executed; one left
  as a slogan does not.
- Not everything can be a law: in that 144-doc run, "doc quality" had no binary
  gate — only a human spot-check. Quality stays a *morality*, and that is the
  honest assignment, not a failure.

**One sentence:** laws are valuable because they are *few and unbreakable*, not
because they are *many and comprehensive* — falsifiable properties become laws,
trendable quantities become metrics, judgment principles become morality; mix
them and one false law discredits the whole system.

---

## 1. Metrics to use

Measure three things independently. A green build alone tells you nothing about
whether TDD happened or whether the test is worth keeping.

### 1.1 Process adherence — "did the agent actually do TDD?"

| Metric | Definition | How to capture | Target |
|---|---|---|---|
| **Red authenticity** | Fraction of slices where a failing test was committed/run *before* the implementation, and the failure was real (assertion failed, not a compile error from a missing symbol being counted as "red"). | Require the agent to paste the failing run output (`--test --test-case=...`) with the test asserting on behavior. | ≥ 90% |
| **Green validity** | Fraction of red tests that reached green by changing *only* production code, not by editing the test or loosening the assertion. | Diff the test file between red and green commits; flag any assertion change that weakens the expectation. | ~100% (test edits to force green = fail) |
| **Iterations to green** | Build/run attempts per slice before the test passes. | Count `scons` + `--test` invocations per slice. | 1–3; >5 is flailing |
| **Cycle time** | Wall-clock per red→green slice. | Timestamp red commit and green commit. | minutes, not hours |
| **Slice size** | Lines of production code changed per test. | `git diff --stat` per slice. | small; large slices = horizontal slicing |

### 1.2 Test quality — "are the tests worth keeping?"

| Metric | Definition | How to capture | Target |
|---|---|---|---|
| **Mutation score** | Fraction of injected mutants (e.g. flip `>`, swap `+`/`-`, early-return) that the test suite kills. Low score = tautological/implementation-coupled tests. | Mutation tool (e.g. `mutate_cpp`, or a hand-rolled mutant list) against the changed code. | ≥ 70% |
| **Coverage delta** | Line/branch coverage of the *changed* code contributed by the new test. | Coverage build (`llvm-cov`/`gcov`; see §1.4). | ≥ 80% on new code |
| **Flake rate** | Fraction of green slices that fail on a clean re-run with no code change. | Run the slice's test 5× back-to-back. | 0% |
| **Tautology check** | The expected value is an independent literal/worked example, not recomputed by the same code path. | Human/AI review of the assertion's right-hand side. | 0 violations |

### 1.3 Delivery outcome — "did it actually ship?"

| Metric | Definition | How to capture | Target |
|---|---|---|---|
| **Finish rate** | Slices that reached "green + review + no regression" out of slices assigned. | Track per-slice status against the Definition of Done in §4.4. | ≥ 90% |
| **Token cost per green test** | LLM tokens (input+output) consumed to land one passing, kept test. | Agent session ledger (OpenWolf `token-ledger.json` or harness log). | trending down |
| **Regression escape** | Bugs later found in the feature that a correct seam-level test should have caught. | Post-hoc: does a reasonable test exist at the seam? | 0 |
| **Stall rate** | Slices where the agent stopped/asked/looped without a green build. | Count interrupted/blocked turns per slice. | <10% |

### 1.4 Capturing coverage (Godot-specific)

Godot builds with SCons and links doctest; it does not ship a one-flag coverage
target in the default `SConstruct`. Use a compiler-level coverage build:

```powershell
# Clang example — emits .profraw; then llvm-profdata/llvm-cov to render.
scons platform=windows target=editor tests=yes use_mingw=no `
  CCFLAGS="-fprofile-instr-generate -fcoverage-mapping" `
  LINKFLAGS="-fprofile-instr-generate"

# Run a single test file's suite, then merge and report.
bin/godot.windows.editor.x86_64.console.exe --test --test-case="[Math]*"
llvm-profdata merge -sparse default.profraw -o merged.profdata
llvm-cov report bin/godot.windows.editor.x86_64.console.exe `
  -instr-profile=merged.profdata core/math/math_funcs.cpp
```

For MSVC use `/fsanitize=coverage` or the `/profile` + `OpenCppCoverage` route;
the coverage *flag* is a build-config concern, the *metric* (coverage delta on
changed code) is what matters.

> **Interpretation rule:** never gate on raw line coverage across the whole
> engine. Gate on the *delta for the code the slice touched*, and weight the
> mutation score higher than coverage — coverage answers "was it run", mutation
> answers "does the test catch a wrong answer".

---

## 2. What test to write

TDD tests behavior at a **seam**: the public boundary where you can observe a
result without reaching into internals. In Godot the seams follow the layered
architecture in `AGENTS.md`. Not every layer is equally testable, so order your
effort by return on investment.

### 2.1 Layer-by-layer testability map

| Layer | Prime seams | Deterministic? | Notes |
|---|---|---|---|
| `core/math` (`Vector2/3`, `AABB`, `Transform2D/3D`, `Quaternion`, `Math` fns) | public operators, transform/math functions | yes | **highest value**; pure functions, easy literals |
| `core/string` (`String`, `StringName`, `NodePath`) | public string/parse/format methods | yes | many existing examples in `tests/core/string` |
| `core/templates` (`Vector`, `HashMap`, `List`, `LocalVector`, `RBMap`, etc.) | container semantics | yes | test invariants and edge sizes (0/1/boundary) |
| `core/variant` (`Variant`, `Array`, `Dictionary`, `Callable`) | variant conversion/coercion rules | yes | `test_macros.h` already stringifies Variants |
| `servers/` (physics/navigation/rendering/audio) | server public API + dummy server | mostly | use suite tags `[Navigation3D]`, `[Audio]`, dummy drivers; never test GPU internals |
| `scene/` (Node, Control, Resources, GUI) | node/public-method behavior | conditional | needs `[SceneTree]` tag to bootstrap `SceneTree` + `DisplayServerMock` |
| `editor/` | editor logic behind public interfaces | conditional | needs `[Editor]` tag + `TOOLS_ENABLED` |
| `modules/` (optional features) | module public API | varies | register via `modules/SCsub`; GDScript tests under `modules/gdscript/tests/scripts` |

### 2.2 Write these tests

1. **Behavioral unit tests at core seams** — math, string, containers, variant.
   Each test names one capability: `"[String] Repeat an empty string"`.
2. **Boundary and edge cases** — empty/zero/one, min/max, NaN, sign, overflow,
   negative, null. These catch the bugs an LLM is most likely to introduce.
3. **Contract tests for script-exposed API** — any new/changed
   `ClassDB`-bound method/property/signal must be exercised through the public
   binding (and `doc/classes/*.xml` regenerated, per `CONTRIBUTING.md`).
4. **Regression tests** — a failing test that reproduces a reported bug *before*
   the fix.
5. **Server behavior via public server API** — e.g. navigation path queries,
   not internal heap layout (see `tests/servers/test_navigation_server_3d.cpp`).

### 2.3 Do NOT write these tests

- **Implementation-detail tests** — mocking internal collaborators, asserting
  call order/counts, or testing private methods. If a refactor without a
  behavior change breaks the test, it's coupled.
- **Tautological tests** — expected value recomputed the same way the code
  computes it (`CHECK(f(a,b) == a + b)` where the code *is* `a + b`). Use a
  known-good literal or a worked example.
- **Rendering/GPU pixel tests** — out of scope for doctest; use the
  render-quality tooling if visual verification is needed.
- **Tests on `thirdparty/`** — vendored, never edited, never unit-tested here.
- **Tests on generated files** — `*.gen.h`, `register_module_types.gen.cpp`,
  `*.compat.inc` are build output.
- **Horizontal slices** — do not write every test first then all the code.
  One test → one minimal implementation → repeat.

---

## 3. Guide to writing a test (Godot specifics)

### 3.1 Where the file goes

Mirror the layer: `tests/<layer>/test_<subject>.cpp`. Examples already in-tree:
`tests/core/math/test_math_funcs.cpp`, `tests/core/string/test_string.cpp`,
`tests/scene/test_button.cpp`. The `tests/SCsub` glob auto-discovers
`*/**/*.cpp`, so **no manual registration is needed** — but the
`TEST_FORCE_LINK` macro is what actually makes the linker pull it in
(`test_builders.py` generates `force_link.gen.h` from the `TEST_FORCE_LINK`
symbols).

### 3.2 Minimal file skeleton

```cpp
/**************************************************************************/
/*  test_<subject>.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_<subject>)

#include "core/math/math_funcs.h" // or the header under test

namespace Test<Subject> {

TEST_CASE("[Math] <one clear behavior>") {
    // Arrange a known-good literal / worked example.
    // Act through the public interface.
    // Assert with CHECK (non-fatal) or REQUIRE (fatal).
    CHECK(Math::abs(-5) == 5);
}

} // namespace Test<Subject>
```

### 3.3 Assertions and naming conventions

- Use `CHECK` (continue on failure) for independent assertions and `REQUIRE`
  (abort) when a later assertion depends on the earlier one holding.
- Name the case `[Area] <behavior>`; the `[Area]` prefix is the suite tag used
  by `--test-suite`/`--test-case` filters and by the listener in
  `tests/test_main.cpp` for special setup:
  - `[SceneTree]` → bootstraps `Input`, `DisplayServerMock`, `SceneTree`,
    physics/navigation dummy servers, `ThemeDB`.
  - `[Editor]` → additionally sets `Engine::set_editor_hint(true)` and creates
    `EditorSettings` (requires a `TOOLS_ENABLED` build).
  - `[Audio]` → initializes the dummy audio driver + `AudioServer`.
  - `[Navigation2D/3D]` → initializes the dummy navigation server.
- Group related cases under a `TEST_SUITE("[Area]") { ... }` block when the
  suite needs one-time setup (see `tests/servers/test_navigation_server_3d.cpp`).

### 3.4 Godot-only helpers available to you

- `ERR_PRINT_OFF` / `ERR_PRINT_ON` — suppress expected engine error spam while
  testing failure paths.
- `ErrorDetector` (in `tests/test_tools.h`) — assert that a specific code path
  raised an engine error, without it printing.
- `TestUtils::get_temp_path(...)`, `get_data_path(...)` — temp/data file access.
- Variant types are pre-stringified in `test_macros.h` (Vector2/3, AABB,
  Transform2D/3D, Dictionary, Array, Packed*Array, etc.) so `CHECK(a == b)`
  prints readable diffs.

### 3.5 Build and run

```powershell
# Build the test binary (Windows, MSVC default; use .console.exe for stdout).
scons platform=windows target=editor tests=yes -j8

# Run everything.
bin/godot.windows.editor.x86_64.console.exe --test

# Run one file / suite / case (doctest CLI passes through, "--test" is stripped).
bin/godot.windows.editor.x86_64.console.exe --test --test-case="[Math]*"
bin/godot.windows.editor.x86_64.console.exe --test --test-suite="core/math"

# List registered tests (sanity-check TEST_FORCE_LINK worked).
bin/godot.windows.editor.x86_64.console.exe --test --list-test-cases
```

### 3.6 When you add/change script-exposed API

If the slice touches a method/property/signal bound via `ClassDB`, regenerate the
class reference and hand-fill descriptions:

```powershell
bin/godot.windows.editor.x86_64.console.exe --doctool
# then edit doc/classes/<ClassName>.xml descriptions; run:
python doc/tools/make_rst.py
```

This is enforced, not optional (`CONTRIBUTING.md:148-158`).

### 3.7 GDScript integration tests (separate runner)

For behavior that only manifests through a script language, use
`modules/gdscript/tests/scripts/` — these are **not** run by the doctest binary;
they are run by the dedicated GDScript test runner (see
`modules/gdscript/tests/README.md`). Do not confuse the two.

---

## 4. How to make an AI agent follow TDD and finish

The failure mode is not "the AI can't write a test." It's that the AI (a)
writes code first and a test after, (b) writes a tautological test, (c) edits
the test to force green, or (d) declares "done" while the build is still red.
The fix is to remove the degrees of freedom that allow those shortcuts.

### 4.1 Freeze the seam before any code

The agent must not pick its own test target mid-loop. Before writing, it writes
a **seam card** and confirms it:

```
Feature: <one capability>
Seam:    <public interface/class + method under test>
Inputs:  <concrete example, from an independent source>
Expected:<known-good literal or worked example>
Tag:     [Area] <behavior name>
```

If the seam is unclear (how deep the module is, where the boundary belongs),
route the agent to the `codebase-design` skill vocabulary first. **No test is
written at an unconfirmed seam.**

### 4.2 Force red before green (the hard gate)

The agent must, in order and with evidence:

1. Write only the test (no production change).
2. Build and run it, and paste the **failing assertion output**.
   - A "red" that is only a *compile error* (missing symbol/header) is **not**
     acceptable red. The test must compile and fail on an assertion.
3. Implement the minimum production code.
4. Re-run and paste the **green output**.
5. Diff-check: the test file must be byte-identical (or only trivially changed)
   between red and green. Any assertion weakened to achieve green = reject.

This gate catches implementation-after-test, tautology, and test-editing.

### 4.3 Vertical slices only

One seam, one test, one minimal implementation per cycle. Forbid "write all
tests, then all code" (horizontal slicing). Each slice is a tracer bullet that
informs the next. Multi-slice features are decomposed into a checklist, and each
item must clear §4.2 before the next begins.

### 4.4 A hard Definition of Done (what "finished" means)

The agent may only mark a slice finished when **all** of these hold, with the
command output pasted as evidence:

- [ ] `scons ... tests=yes` returns exit 0.
- [ ] `--test --test-case="<this case>"` is green.
- [ ] The full suite still passes (`--test`), or at minimum the touched area's
      suite passes — no regressions.
- [ ] The test is behavioral (public seam, independent expected value), not
      tautological or implementation-coupled.
- [ ] `pre-commit run --all-files` passes (clang-format, ruff, codespell) if the
      slice touched tracked files.
- [ ] If script-exposed API changed: `doc/classes/*.xml` regenerated + filled.
- [ ] Red→green evidence archived (failing output + green output + test diff).

An agent that says "done" without this list is **not done** — treat it as
in-progress and re-inject the objective.

### 4.5 Anti-drift: re-inject the full objective every turn

AI agents drift across long sessions. Use a persistent goal with an anti-drift
steering prompt that restates the complete objective and the current slice at
the start of every turn. The agent must not:

- declare victory after one passing test when the feature has remaining seams;
- silently skip the red phase to save time;
- "improve" the implementation beyond the minimum needed to pass the current
  test (no speculative features);
- treat "the test passes" as permission to skip review.

Completion is audited against real build/test state, not the agent's claim.
Block only after a *recurring* genuine blocker (≥3 consecutive turns on the same
blocker), not because the work is slow or hard.

### 4.6 A reusable TDD prompt (paste as the agent's task)

```text
You are doing strict TDD in the godot repo. Follow the red→green loop, one
vertical slice at a time. Do NOT write production code before its failing test.
Do NOT write a test whose expected value is recomputed the same way as the code.
Do NOT edit a test to make it pass.

For each slice:
1. Emit a seam card (feature / seam / inputs / expected literal / [Area] name)
   and wait for confirmation before writing.
2. Write ONLY the test at tests/<layer>/test_<subject>.cpp, with TEST_FORCE_LINK.
3. Build and run it; paste the failing ASSERTION output (compile errors do not
   count as red).
4. Implement the minimum production code to make it pass.
5. Paste the green output and confirm the test file did not change between
   red and green.
6. Report against the Definition of Done checklist and do not claim "done"
   until every box is checked and evidenced.
```

### 4.7 Review is outside the loop

Refactoring is not part of the red→green cycle — it happens after green, at the
review stage (`code-review` skill), where the test is judged for worthiness
(seam-correct, non-tautological, survives refactor) before merge. Keep
implementation and cleanup separated in the agent's mind so it doesn't refactor
mid-slice and lose the red/green traceability.

---

## 5. One-page checklist (print/tuck in the prompt)

**Before** — freeze seam → confirm with user → write seam card.
**Red** — test only → build → run → failing *assertion* pasted.
**Green** — minimal impl → build → run → green pasted → test diff clean.
**Done** — exit 0 build + targeted green + no regression + behavioral test +
lint/format + doc regen (if API) + evidence archived.
**Never** — code before test · tautological expected value · test-editing to
green · horizontal slicing · refactor mid-loop · "done" while red.
